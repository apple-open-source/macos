/* $Xorg: mpcomp.c,v 1.6 2001/02/09 02:04:30 xorgcvs Exp $ */
/**** module mpcomp.c ****/
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
  
	mpcomp.c -- DDXIE compare element
  
	Larry Hare -- AGE Logic, Inc. August, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mpcomp.c,v 3.5 2001/12/14 19:58:44 dawes Exp $ */


#define _XIEC_MPCOMP
#define _XIEC_PCOMP

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

/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzeCompare();

/*
**  Turn on NON_ROI to get non ROI based versions.  They don't really go
**  faster and cost about 20Kbytes of object code space.
*/

/*
 *  routines used internal to this module
 */
static int CreateCompare();
static int InitializeCompare();
static int ResetCompare();
static int DestroyCompare();

#if defined(NON_ROI)
static int ActivateCompareM();
static int ActivateCompareD();
#endif
static int ActivateCompareMROI();
static int ActivateCompareDROI();
static int ActivateCompareTripleM();
static int ActivateCompareTripleD();

/*
 * DDXIE Compare entry points
 */
static ddElemVecRec CompareVec = {
  CreateCompare,
  InitializeCompare,
  ActivateCompareMROI,
  (xieBoolProc)NULL,
  ResetCompare,
  DestroyCompare
  };

/*
* Local Declarations.
*/

typedef struct _mpcomparedef {
    CARD32	iconst;			/* first, in case of .asm code */
    RealPixel	fconst;			/* second, in case of .asm code */
    void	(*action) ();
    CARD32	width;
    BOOL	final;
#if defined(NON_ROI)
    void	(*action2) ();
    CARD32	endrun;
    CARD32	endix;
#endif
} mpComparePvtRec, *mpComparePvtPtr;

/*
** NOTE:  Might change constants to use dyads with prefilled constant strip
**	  to conserve code space at some small execution time expense.
** NOTE:  The ROI variants use three loops, the first to get in sync with
**	  32 bits, and the last to finish up partial words.  A more space
**	  efficient version would have one loop which would check for M
**	  wrapping in which case the old word would be written.
**	  It also requires loads of intermediate destination words which
**	  will be completely overwritten.
** NOTE:  Might do NE/GT/GE as a second pass inversion of EQ/LE/LT to
**	  save even more space at cost of another time penalty.  This 
**	  inversion would have to be applied to active ROI entries only.
** NOTE:  On some machine architectures, it may be possible to write
**	  assembly or asm() code which does a compare and then extracts
**	  some 1 bit from a condition code register.  This would reduce
**	  the multiplication of code by 6 (or 3).  Heck you could even
**	  generate assembly code yourself automagically ...
** NOTE:  Triple Compare assumes all src1 bands same height/width.
** NOTE:  Triple Compare assumes all src2 bands same height/width.
** NOTE:  Triple Compare with band_mask of 0 sets output to 1 (for
**	  equal) or 0 (for not equal) or 0 (outside ROI).  This sortof
**	  makes sense since all the selected bands were equal :-)
*/

/*------------------------------------------------------------------------
------------------------  fill in the vector  ---------------------------
------------------------------------------------------------------------*/
int miAnalyzeCompare(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    ped->ddVec = CompareVec;
    return TRUE;
}

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateCompare(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    /* always force syncing between inputs (is nop if only one input) */
    return MakePETex(flo,ped,
		     xieValMaxBands * sizeof(mpComparePvtRec),
		     SYNC,	/* InSync: Make sure ROI exists first */
		     NO_SYNC	/* bandSync: see InitializeCompare */
		     );
} 

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/

#if defined(NON_ROI)
static int ActivateCompareM(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpComparePvtPtr pvt = (mpComparePvtPtr) pet->private;
    int band, nbands = pet->receptor[SRCt1].inFlo->bands;
    bandPtr sband = &(pet->receptor[SRCt1].band[0]);
    bandPtr dband = &(pet->emitter[0]);

    for(band = 0; band < nbands; band++, pvt++, sband++, dband++) {
	CARD32 npix = pvt->width;
	LogInt *svoid, *dvoid;

    	if (!(svoid = GetCurrentSrc(flo,pet,sband)) ||
	    !(dvoid = GetCurrentDst(flo,pet,dband))) continue;

	do {
	    /* NOTE: could pass in replicated constant strip here */
	    (*(pvt->action)) (dvoid, svoid, pvt, npix);
	    svoid = GetNextSrc(flo,pet,sband,FLUSH);
	    dvoid = GetNextDst(flo,pet,dband,FLUSH);
	} while (!ferrCode(flo) && svoid && dvoid) ;

	FreeData(flo, pet, sband, sband->current);
    }
    return TRUE;
}
#endif /* NON_ROI */

#if defined(NON_ROI)
static int ActivateCompareD(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpComparePvtPtr pvt = (mpComparePvtPtr) pet->private;
    int band, nbands = pet->receptor[SRCt1].inFlo->bands;
    bandPtr sband = &(pet->receptor[SRCt1].band[0]);
    bandPtr tband = &(pet->receptor[SRCt2].band[0]);
    bandPtr dband = &(pet->emitter[0]);

    for(band = 0; band < nbands; band++, pvt++, sband++, tband++, dband++) {
	LogInt *svoid, *tvoid, *dvoid;

    	if (!(svoid = GetCurrentSrc(flo,pet,sband)) ||
	    !(tvoid = GetCurrentSrc(flo,pet,tband)) ||
	    !(dvoid = GetCurrentDst(flo,pet,dband))   ) {
	    if (sband->final && tband->final) {
		/* Generate constant fill of 0 for remainder of image */
		while ((dband->current < dband->format->height) &&
	    	       (dvoid = GetCurrentDst(flo,pet,dband))) {
			action_clear(dvoid,dband->format->pitch,0);
			if (PutData(flo,pet,dband,dband->current+1))
			    break;
		}
	    }
	    continue;
	}

	do {
	    (*(pvt->action)) (dvoid, svoid, tvoid, pvt->endix);
	    if (pvt->action2)
		(*(pvt->action2)) (dvoid, pvt->endrun, pvt->endix);
	    svoid = GetNextSrc(flo,pet,sband,FLUSH);
	    tvoid = GetNextSrc(flo,pet,tband,FLUSH);
	    dvoid = GetNextDst(flo,pet,dband,FLUSH);
	} while (!ferrCode(flo) && svoid && tvoid && dvoid) ;

	if(!svoid && sband->final) {	/* when sr1 runs out, kill sr2 too  */
	    DisableSrc(flo,pet,tband,FLUSH);
	    pvt->final = TRUE; /* generate constant fill on future activates */
	} else if(!tvoid && tband->final) {/* when sr2 runs out, clear rest */
	    DisableSrc(flo,pet,sband,FLUSH);
	    pvt->final = TRUE; /* generate constant fill on future activates */
	} else { /* both inputs still active, keep the scheduler up to date  */
	    FreeData(flo,pet,sband,sband->current);
	    FreeData(flo,pet,tband,tband->current);
	}
    }
    return TRUE;
}
#endif /* NON_ROI */

static int ActivateCompareMROI(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpComparePvtPtr pvt = (mpComparePvtPtr) pet->private;
    int band, nbands  = pet->receptor[SRCt1].inFlo->bands;
    bandPtr sband     = &(pet->receptor[SRCt1].band[0]);
    bandPtr dband     = &(pet->emitter[0]);

    for(band = 0; band < nbands; band++, pvt++, sband++, dband++) {
	pointer svoid, dvoid;

    	if (!(svoid = GetCurrentSrc(flo,pet,sband)) ||
	    !(dvoid = GetCurrentDst(flo,pet,dband))) continue;

	while (!ferrCode(flo) && svoid && dvoid && 
				SyncDomain(flo,ped,dband,FLUSH)) {
	    INT32 run, ix = 0;
	   
	    /* bzero better for messy control planes */
	    /* bzero((char *)dvoid, dband->pitch); */
	    while (run = GetRun(flo,pet,dband)) {
		if (run > 0) {
	    	    /* NOTE: could pass in replicated constant strip here */
	    	    (*(pvt->action)) (dvoid, svoid, pvt, run, ix);
		    ix += run;
		} else {
		    /* action_clear better for non ROI or simple ROIs */
		    action_clear(dvoid, -run, ix);
		    ix -= run;
		}
	    }
	    svoid = GetNextSrc(flo,pet,sband,FLUSH);
	    dvoid = GetNextDst(flo,pet,dband,FLUSH);
	}

	FreeData(flo, pet, sband, sband->current);
    }
    return TRUE;
}

static int ActivateCompareDROI(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpComparePvtPtr pvt = (mpComparePvtPtr) pet->private;
    int band, nbands  = pet->receptor[SRCt1].inFlo->bands;
    bandPtr sband     = &(pet->receptor[SRCt1].band[0]);
    bandPtr tband     = &(pet->receptor[SRCt2].band[0]);
    bandPtr dband     = &(pet->emitter[0]);

    for(band = 0; band < nbands; band++, pvt++, sband++, tband++, dband++) {
	pointer svoid, tvoid, dvoid;
	CARD32 w = pvt->width;

    	if (!(svoid = GetCurrentSrc(flo,pet,sband)) ||
    	    !(tvoid = GetCurrentSrc(flo,pet,tband)) ||
	    !(dvoid = GetCurrentDst(flo,pet,dband))) {
	    if (sband->final && tband->final) {
		/* Generate constant fill of 0 for remainder of image */
		while ((dband->current < dband->format->height) &&
	    	       (dvoid = GetCurrentDst(flo,pet,dband))) {
			action_clear(dvoid,dband->format->pitch,0);
			if (PutData(flo,pet,dband,dband->current+1))
			    break;
		}
	    }
	    continue;
	}

	while (!ferrCode(flo) && svoid && tvoid && dvoid && 
				SyncDomain(flo,ped,dband,FLUSH)) {
	    INT32 run, ix = 0;
	   
	    /* bzero better for messy control planes */
	    /* bzero((char *)dvoid, dband->pitch); */
	    while (run = GetRun(flo,pet,dband)) {
		if (run > 0) {
		    /* needs to clip to second source, yuck */
		    if ((ix + run) > w) {
			if (ix < w) {
			    (*(pvt->action)) (dvoid, svoid, tvoid, w - ix, ix);
			    ix = w;
			}
		        action_clear(dvoid, dband->format->width - ix, ix);
			break;
		    }
	    	    (*(pvt->action)) (dvoid, svoid, tvoid, run, ix);
		    ix += run;
		} else {
		    /* action_clear better for non ROI or simple ROIs */
		    action_clear(dvoid, -run, ix);
		    ix -= run; 
		}
	    }
	    svoid = GetNextSrc(flo,pet,sband,FLUSH);
	    tvoid = GetNextSrc(flo,pet,tband,FLUSH);
	    dvoid = GetNextDst(flo,pet,dband,FLUSH);
	}

	if(!svoid && sband->final) {
	    /* when sr1 runs out, kill sr2 too. should be done */
	    DisableSrc(flo,pet,tband,FLUSH);
	    pvt->final = TRUE;
	} else if(!tvoid && tband->final) {
	    /* when sr2 runs out, kill off sr1 also, and clear remaining dest */
	    DisableSrc(flo,pet,sband,FLUSH);
	    pvt->final = TRUE;
	} else {
	    /* both inputs still active, keep the scheduler up to date  */
	    FreeData(flo,pet,sband,sband->current);
	    FreeData(flo,pet,tband,tband->current);
	}
    }
    return TRUE;
}

static int ActivateCompareTripleM(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpComparePvtPtr pvt = (mpComparePvtPtr) pet->private;
    bandPtr	 sband = &(pet->receptor[SRCt1].band[0]);
    bandPtr	 dband = &(pet->emitter[0]);
    CARD8 	   msk = ((xieFloCompare *) ped->elemRaw)->bandMask;
    BOOL	 equal = ((xieFloCompare *) ped->elemRaw)->operator == xieValEQ;
    register pointer s0 = (pointer ) 1;
    register pointer s1 = (pointer ) 1;
    register pointer s2 = (pointer ) 1;
    register pointer dvoid;

    if ((msk & 1) && !(s0 = GetCurrentSrc(flo,pet,sband)))
	goto done; sband++;
    if ((msk & 2) && !(s1 = GetCurrentSrc(flo,pet,sband)))
	goto done; sband++;
    if ((msk & 4) && !(s2 = GetCurrentSrc(flo,pet,sband)))
	goto done; sband -= 2;
    if(!(dvoid = GetCurrentDst(flo,pet,dband)))
	goto done;

    /*
    **  An alternative algorithm might use while(GetRun) loop to
    **  set or clear the destination bits in the line; and then
    **  use SIMPLER action routines which scanned the entire line
    **  and only operate on 1 pixels; optionally followed by
    **  a while(GetRun) to invert the active bits for NE.
    */
    pvt += 2;
    while (!ferrCode(flo) && s0 && s1 && s2 && dvoid && 
			SyncDomain(flo,ped,dband,FLUSH)) {
	register INT32 run, ix = 0;

	while (run = GetRun(flo,pet,dband)) {
	    if (run > 0) {
		action_set(dvoid, run, ix);
		pvt -= 2; if (msk & 1)
			(*pvt->action) (dvoid, s0, pvt, run, ix);
		pvt++; if (msk & 2)
			(*pvt->action) (dvoid, s1, pvt, run, ix);
		pvt++; if (msk & 4)
			(*pvt->action) (dvoid, s2, pvt, run, ix);
		if (!equal)
			action_invert(dvoid, run, ix);
		ix += run;
	    } else {
		action_clear(dvoid, -run, ix);
		ix -= run;
	    }
	}

	if (msk & 1) s0 = GetNextSrc(flo,pet,sband,FLUSH); sband++;
	if (msk & 2) s1 = GetNextSrc(flo,pet,sband,FLUSH); sband++;
	if (msk & 4) s2 = GetNextSrc(flo,pet,sband,FLUSH); sband -= 2;
	dvoid = GetNextDst(flo,pet,dband,FLUSH);
    }

    if (msk & 1) FreeData(flo,pet,sband,sband->current); sband++;
    if (msk & 2) FreeData(flo,pet,sband,sband->current); sband++;
    if (msk & 4) FreeData(flo,pet,sband,sband->current); sband -= 2;
done:
    return TRUE;
}

static int ActivateCompareTripleD(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
    mpComparePvtPtr pvt = (mpComparePvtPtr) pet->private;
    bandPtr      sband = &(pet->receptor[SRCt1].band[0]);
    bandPtr      tband = &(pet->receptor[SRCt2].band[0]);
    bandPtr      dband = &(pet->emitter[0]);
    CARD8	   msk = ((xieFloCompare *) ped->elemRaw)->bandMask;
    BOOL  	 equal = ((xieFloCompare *) ped->elemRaw)->operator == xieValEQ;
    CARD32 	     w = pvt->width;
    register pointer s0 = (pointer ) 1;
    register pointer s1 = (pointer ) 1;
    register pointer s2 = (pointer ) 1;
    register pointer t0 = (pointer ) 1;
    register pointer t1 = (pointer ) 1;
    register pointer t2 = (pointer ) 1;
    register pointer dvoid;

    if (pvt->final) {  /* generate constant fill */
	/* Generate constant fill of 0 for remainder of image */
	while ((dband->current < dband->format->height) &&
	       (dvoid = GetCurrentDst(flo,pet,dband))) {
		action_clear(dvoid,dband->format->pitch,0);
		if (PutData(flo,pet,dband,dband->current+1))
		    break;
	}
	return TRUE;
    }

    if (msk & 1) s0 = GetCurrentSrc(flo,pet,sband); sband++;
    if (msk & 2) s1 = GetCurrentSrc(flo,pet,sband); sband++;
    if (msk & 4) s2 = GetCurrentSrc(flo,pet,sband); sband -= 2;
    if (msk & 1) t0 = GetCurrentSrc(flo,pet,tband); tband++;
    if (msk & 2) t1 = GetCurrentSrc(flo,pet,tband); tband++;
    if (msk & 4) t2 = GetCurrentSrc(flo,pet,tband); tband -= 2;

    dvoid = GetCurrentDst(flo,pet,dband);

    while (!ferrCode(flo) && s0 && s1 && s2 && t0 && t1 && t2 && dvoid && 
			SyncDomain(flo,ped,dband,FLUSH)) {
	register INT32 run, ix = 0, extra;

	while (run = GetRun(flo,pet,dband)) {
	    if (run > 0) {
	        extra = 0;
		if ((ix+run) > w) { /* tband < sband */
		    if (ix >= w) { /* already off the end of src2 */ 
			action_clear(dvoid, dband->format->width - ix, ix);
			break;
		    }
		    extra = ix + run - w;
		    action_clear(dvoid, extra, w);
		    run = w - ix;
		}
		action_set(dvoid, run, ix);
		if (msk & 1) (*((pvt+0)->action)) (dvoid, s0, t0, run, ix); 
		if (msk & 2) (*((pvt+1)->action)) (dvoid, s1, t1, run, ix); 
		if (msk & 4) (*((pvt+2)->action)) (dvoid, s2, t2, run, ix); 
		if (!equal)  action_invert(dvoid, run, ix);
		ix += run + extra;
	    } else {
		action_clear(dvoid, -run, ix);
		ix -= run;
	    }
	}

	if (msk & 1) s0 = GetNextSrc(flo,pet,sband,FLUSH); sband++;
	if (msk & 2) s1 = GetNextSrc(flo,pet,sband,FLUSH); sband++;
	if (msk & 4) s2 = GetNextSrc(flo,pet,sband,FLUSH); sband -= 2;
	if (msk & 1) t0 = GetNextSrc(flo,pet,tband,FLUSH); tband++;
	if (msk & 2) t1 = GetNextSrc(flo,pet,tband,FLUSH); tband++;
	if (msk & 4) t2 = GetNextSrc(flo,pet,tband,FLUSH); tband -= 2;
	dvoid = GetNextDst(flo,pet,dband,FLUSH);
    }

    if (msk & 1) FreeData(flo,pet,sband,sband->current); sband++;
    if (msk & 2) FreeData(flo,pet,sband,sband->current); sband++;
    if (msk & 4) FreeData(flo,pet,sband,sband->current); sband -= 2;
    if (msk & 1) FreeData(flo,pet,tband,tband->current); tband++;
    if (msk & 2) FreeData(flo,pet,tband,tband->current); tband++;
    if (msk & 4) FreeData(flo,pet,tband,tband->current); tband -= 2;

    /* if dband is final, then we are done. make sure src2 disabled */
    if (dband->current >= dband->format->height) {
	if (msk & 1) DisableSrc(flo,pet,tband,FLUSH); tband++;
	if (msk & 2) DisableSrc(flo,pet,tband,FLUSH); tband++;
	if (msk & 4) DisableSrc(flo,pet,tband,FLUSH); tband -= 2;
	return TRUE;
    }

    /* if src2 finished early, then disable src1 and generate 0 fill */
    if (dband->current >= tband->format->height) {
    	/* only need to check one band since in theory all the same */ 
	if (msk & 1) DisableSrc(flo,pet,sband,FLUSH); sband++;
	if (msk & 2) DisableSrc(flo,pet,sband,FLUSH); sband++;
	if (msk & 4) DisableSrc(flo,pet,sband,FLUSH); sband -= 2;
	pvt->final = TRUE;
    }
    return TRUE;
}

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetCompare(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    ResetReceptors(ped);
    ResetProcDomain(ped);
    ResetEmitter(ped);
    return TRUE;
}

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyCompare(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
    /* get rid of the peTex structure  */
    ped->peTex = (peTexPtr) XieFree(ped->peTex);

    /* zap this element's entry point vector */
    ped->ddVec.create = (xieIntProc)NULL;
    ped->ddVec.initialize = (xieIntProc)NULL;
    ped->ddVec.activate = (xieIntProc)NULL;
    ped->ddVec.reset = (xieIntProc)NULL;
    ped->ddVec.destroy = (xieIntProc)NULL;

    return TRUE;
} 

/*------------------------------------------------------------------------
---------------------  Lotsa Little Action Routines  ---------------------
------------------------------------------------------------------------*/

#if defined(NON_ROI)
/* M:	(*(pvt->action)) (dvoid, svoid, pvt, bw); */
/* D:	(*(pvt->action)) (dvoid, svoid, tvoid, bw); */

#define MakeBit(name1, name2, op) 					\
static void name1(dst,src1,pvt,nx)					\
    LogInt *dst;							\
    pointer src1;							\
    mpComparePvtPtr pvt;						\
    CARD32 nx;								\
{									\
    LogInt *src = (LogInt *) src1;					\
    LogInt S1, S2 = (pvt->iconst ? LOGONES : LOGZERO);			\
    for (nx = (nx + LOGMASK) >> LOGSHIFT; nx > 0; nx--) {		\
	S1 = *src++;							\
	*dst++ = op;							\
    }									\
}									\
static void name2(dst,src1,src2,nx)					\
    LogInt *dst;							\
    pointer src1, src2;							\
    CARD32 nx;								\
{									\
    LogInt S1, *src = (LogInt *) src1;					\
    LogInt S2, *trc = (LogInt *) src2;					\
    for (nx = (nx + LOGMASK) >> LOGSHIFT; nx > 0; nx--) {		\
	S1 = *src++;							\
	S2 = *trc++;							\
	*dst++ = op;							\
    }									\
}

#define MakePix(name1, name2, itype, cnst_name, op)			\
static void name1(dst,src1,pvt,nx)					\
    LogInt *dst;							\
    pointer src1;							\
    mpComparePvtPtr pvt;						\
    CARD32 nx;								\
{									\
    itype *src = (itype *) src1;					\
    itype con = (itype) pvt->cnst_name;					\
    LogInt M, value;							\
    for ( ; nx >= LOGSIZE; nx -= LOGSIZE, *dst++ = value) 		\
	for (value = 0, M=LOGLEFT; M ; LOGRIGHT(M))			\
	    if (*src++ op con)						\
		value |= M;						\
    if (nx > 0) {							\
	for (value = 0, M=LOGLEFT; nx; nx--, LOGRIGHT(M))		\
	    if (*src++ op con)						\
		value |= M;						\
	*dst = value;							\
    }									\
}									\
static void name2(dst,src1,src2,nx)					\
    LogInt *dst;							\
    pointer src1, src2;							\
    CARD32 nx;								\
{									\
    itype *src = (itype *) src1;					\
    itype *trc = (itype *) src2;					\
    LogInt M, value;							\
    for ( ; nx >= LOGSIZE; nx -= LOGSIZE, *dst++ = value) 		\
	for (value = 0, M=LOGLEFT; M ; LOGRIGHT(M))			\
	    if (*src++ op *trc++)					\
		value |= M;						\
    if (nx > 0) {							\
	for (value = 0, M=LOGLEFT; nx; nx--, LOGRIGHT(M))		\
	    if (*src++ op *trc++)					\
		value |= M;						\
	*dst = value;							\
    }									\
}

MakeBit	(m_bit_lt, d_bit_lt, (S2 & ~S1) )
MakeBit	(m_bit_le, d_bit_le, (S2 | ~S1) )
MakeBit	(m_bit_eq, d_bit_eq, (S1 ^ ~S2)	)
MakeBit	(m_bit_ne, d_bit_ne, (S1 ^  S2)	)
MakeBit	(m_bit_gt, d_bit_gt, (S1 & ~S2)	)
MakeBit	(m_bit_ge, d_bit_ge, (S1 | ~S2) )

MakePix	(m_byte_lt, d_byte_lt, BytePixel, iconst, <  )
MakePix	(m_byte_le, d_byte_le, BytePixel, iconst, <= )
MakePix	(m_byte_eq, d_byte_eq, BytePixel, iconst, == )
MakePix	(m_byte_ne, d_byte_ne, BytePixel, iconst, != )
MakePix	(m_byte_gt, d_byte_gt, BytePixel, iconst, >  )
MakePix	(m_byte_ge, d_byte_ge, BytePixel, iconst, >= )

MakePix	(m_pair_lt, d_pair_lt, PairPixel, iconst, <  )
MakePix	(m_pair_le, d_pair_le, PairPixel, iconst, <= )
MakePix	(m_pair_eq, d_pair_eq, PairPixel, iconst, == )
MakePix	(m_pair_ne, d_pair_ne, PairPixel, iconst, != )
MakePix	(m_pair_gt, d_pair_gt, PairPixel, iconst, >  )
MakePix	(m_pair_ge, d_pair_ge, PairPixel, iconst, >= )

MakePix	(m_quad_lt, d_quad_lt, QuadPixel, iconst, <  )
MakePix	(m_quad_le, d_quad_le, QuadPixel, iconst, <= )
MakePix	(m_quad_eq, d_quad_eq, QuadPixel, iconst, == )
MakePix	(m_quad_ne, d_quad_ne, QuadPixel, iconst, != )
MakePix	(m_quad_ge, d_quad_ge, QuadPixel, iconst, >= )
MakePix	(m_quad_gt, d_quad_gt, QuadPixel, iconst, >  )

MakePix	(m_real_lt, d_real_lt, RealPixel, fconst, <  )
MakePix	(m_real_le, d_real_le, RealPixel, fconst, <= )
MakePix	(m_real_eq, d_real_eq, RealPixel, fconst, == )
MakePix	(m_real_ne, d_real_ne, RealPixel, fconst, != )
MakePix	(m_real_gt, d_real_gt, RealPixel, fconst, >  )
MakePix	(m_real_ge, d_real_ge, RealPixel, fconst, >= )

static void (*action_mono[5][6])() = {
	m_real_lt, m_real_le, m_real_eq, m_real_ne, m_real_gt, m_real_ge,
	 m_bit_lt,  m_bit_le,  m_bit_eq,  m_bit_ne,  m_bit_gt,  m_bit_ge,
	m_byte_lt, m_byte_le, m_byte_eq, m_byte_ne, m_byte_gt, m_byte_ge,
	m_pair_lt, m_pair_le, m_pair_eq, m_pair_ne, m_pair_gt, m_pair_ge,
	m_quad_lt, m_quad_le, m_quad_eq, m_quad_ne, m_quad_gt, m_quad_ge
};
static void (*action_dyad[5][6])() = {
	d_real_lt, d_real_le, d_real_eq, d_real_ne, d_real_gt, d_real_ge,
	d_bit_lt,  d_bit_le,  d_bit_eq,  d_bit_ne,  d_bit_gt,  d_bit_ge,
	d_byte_lt, d_byte_le, d_byte_eq, d_byte_ne, d_byte_gt, d_byte_ge,
	d_pair_lt, d_pair_le, d_pair_eq, d_pair_ne, d_pair_gt, d_pair_ge,
	d_quad_lt, d_quad_le, d_quad_eq, d_quad_ne, d_quad_gt, d_quad_ge
};
#endif

/*------------------------------------------------------------------------
---------------------  ROI operations work on subranges ------------------
------------------------------------------------------------------------*/

/* MROI: (*(pvt->action)) (dvoid, src1, pvt, run, ix); */
/* DROI: (*(pvt->action)) (dvoid, src1, src2, run, ix); */

/* Currently ROI code pre-zeroes the line and then we OR into it.
** If we want to use this for the nonROI code as well, it might be
** more efficient to instead use a passive routine (when run < 0) to
** clear out the idle bits.
*/

#define RakeBit(name1, name2, op) 					\
static void name1(dst,src1,pvt,dx,x)					\
    LogInt *dst;							\
    pointer src1;							\
    mpComparePvtPtr pvt;						\
    INT32  dx, x;							\
{									\
    CARD32 M, D, px = LOGINDX(x);					\
    LogInt S1, *src = ((LogInt *) src1) + px;				\
    LogInt S2 = (pvt->iconst ? LOGONES : LOGZERO);			\
    dst += px; 								\
    px = x & LOGMASK;							\
    if ((px + dx) >= LOGSIZE) {						\
	if (px) {							\
	    S1 = *src++; 						\
	    D = *dst;							\
	    M = BitRight(LOGONES,px);					\
	    dx -= (LOGSIZE - px);					\
	    *dst++ = D | (M & op);					\
	}								\
	for (px = dx >> LOGSHIFT; px > 0; px--) {			\
	    S1 = *src++; 						\
	    *dst++ = op;						\
	}								\
	if (dx &= LOGMASK) {						\
	    S1 = *src; 							\
	    D = *dst;							\
	    M = ~BitRight(LOGONES,dx);					\
	    *dst = D | (M & op);					\
	}								\
    } else {								\
	    S1 = *src;	 						\
	    D = *dst;							\
	    M = BitRight(LOGONES,px) & ~(BitRight(LOGONES,px+dx));	\
	    *dst = D | (M & op);					\
    }									\
}									\
static void name2(dst,src1,src2,dx,x)					\
    LogInt *dst;							\
    pointer src1, src2;							\
    INT32  dx, x;							\
{									\
    CARD32 M, D, px = LOGINDX(x);					\
    LogInt S1, *src = ((LogInt *) src1) + px;				\
    LogInt S2, *trc = ((LogInt *) src2) + px;				\
    dst += px; 								\
    px = x & LOGMASK;							\
    if ((px + dx) >= LOGSIZE) {						\
	if (px) {							\
	    S1 = *src++; S2 = *trc++;					\
	    D = *dst;							\
	    M = BitRight(LOGONES,px);					\
	    dx -= (LOGSIZE - px);					\
	    *dst++ = D | (M & op);					\
	}								\
	for (px = dx >> LOGSHIFT; px > 0; px--) {			\
	    S1 = *src++; S2 = *trc++;					\
	    *dst++ = op;						\
	}								\
	if (dx &= LOGMASK) {						\
	    S1 = *src; S2 = *trc;					\
	    D = *dst;							\
	    M = ~BitRight(LOGONES,dx);					\
	    *dst = D | (M & op);					\
	}								\
    } else {								\
	    S1 = *src; S2 = *trc;					\
	    D = *dst;							\
	    M = BitRight(LOGONES,px) & ~(BitRight(LOGONES,px+dx));	\
	    *dst = D | (M & op);					\
    }									\
}

#define RakePix(name1, name2, itype, cnst_name, op)			\
static void name1(dst,src1,pvt,dx,x)					\
    LogInt *dst;							\
    pointer src1;							\
    mpComparePvtPtr pvt;						\
    INT32  dx, x;							\
{									\
    itype *src = ((itype *) src1) + x;					\
    itype con = (itype) pvt->cnst_name;					\
    LogInt M, value;							\
    dst += LOGINDX(x); 							\
    if (x & LOGMASK) {							\
	for (value = *dst, M=LOGBIT(x); dx && M; dx--, LOGRIGHT(M))	\
	    if (*src++ op con)						\
		value |= M;						\
	    else							\
		value &= ~M;						\
	*dst++ = value;							\
    }									\
    for ( ; dx >= LOGSIZE; dx -= LOGSIZE, *dst++ = value) 		\
	for (value = 0, M=LOGLEFT; M ; LOGRIGHT(M))			\
	    if (*src++ op con)						\
		value |= M;						\
    if (dx > 0) {							\
	for (value = 0, M=LOGLEFT; dx; dx--, LOGRIGHT(M))		\
	    if (*src++ op con)						\
		value |= M;						\
	*dst = value;							\
    }									\
}									\
static void name2(dst,src1,src2,dx,x)					\
    LogInt *dst;							\
    pointer src1, src2;							\
    INT32  dx, x;							\
{									\
    itype *src = ((itype *) src1) + x;					\
    itype *trc = ((itype *) src2) + x;					\
    LogInt M, value;							\
    dst += LOGINDX(x); 							\
    if (x & LOGMASK) {							\
	for (value = *dst, M=LOGBIT(x); dx && M; dx--, LOGRIGHT(M))	\
	    if (*src++ op *trc++)					\
		value |= M;						\
	    else							\
		value &= ~M;						\
	*dst++ = value;							\
    }									\
    for ( ; dx >= LOGSIZE; dx -= LOGSIZE, *dst++ = value) 		\
	for (value = 0, M=LOGLEFT; M ; LOGRIGHT(M))			\
	    if (*src++ op *trc++)					\
		value |= M;						\
    if (dx > 0) {							\
	for (value = 0, M=LOGLEFT; dx; dx--, LOGRIGHT(M))		\
	    if (*src++ op *trc++)					\
		value |= M;						\
	*dst = value;							\
    }									\
}

/*
** Consider the following paradigm.  It adds a taken branch to 
** each loop, but reduces the amount of total code.  This mechanism
** also relies on the preclearing of the full destination while
** the above mechanism could be adapted to apply boundary masks,
** to work more efficiently with nonROI data.
**
**	for (value = 0, M = x & LOGMASK ; dx ; dx--) {
**	    if (*src++ op *trc++) value |= M;
**	    if (LOGRIGHT(M)) { *dst++ |= value; value = 0; }
**	}		
*/

RakeBit	(rm_bit_lt, rd_bit_lt, (S2 & ~S1) )
RakeBit	(rm_bit_le, rd_bit_le, (S2 | ~S1) )
RakeBit	(rm_bit_eq, rd_bit_eq, (S1 ^ ~S2) )
RakeBit	(rm_bit_ne, rd_bit_ne, (S1 ^  S2) )
RakeBit	(rm_bit_gt, rd_bit_gt, (S1 & ~S2) )
RakeBit	(rm_bit_ge, rd_bit_ge, (S1 | ~S2) )

RakePix	(rm_byte_lt, rd_byte_lt, BytePixel, iconst, <  )
RakePix	(rm_byte_le, rd_byte_le, BytePixel, iconst, <= )
RakePix	(rm_byte_eq, rd_byte_eq, BytePixel, iconst, == )
RakePix	(rm_byte_ne, rd_byte_ne, BytePixel, iconst, != )
RakePix	(rm_byte_gt, rd_byte_gt, BytePixel, iconst, >  )
RakePix	(rm_byte_ge, rd_byte_ge, BytePixel, iconst, >= )

RakePix	(rm_pair_lt, rd_pair_lt, PairPixel, iconst, <  )
RakePix	(rm_pair_le, rd_pair_le, PairPixel, iconst, <= )
RakePix	(rm_pair_eq, rd_pair_eq, PairPixel, iconst, == )
RakePix	(rm_pair_ne, rd_pair_ne, PairPixel, iconst, != )
RakePix	(rm_pair_gt, rd_pair_gt, PairPixel, iconst, >  )
RakePix	(rm_pair_ge, rd_pair_ge, PairPixel, iconst, >= )

RakePix	(rm_quad_lt, rd_quad_lt, QuadPixel, iconst, <  )
RakePix	(rm_quad_le, rd_quad_le, QuadPixel, iconst, <= )
RakePix	(rm_quad_eq, rd_quad_eq, QuadPixel, iconst, == )
RakePix	(rm_quad_ne, rd_quad_ne, QuadPixel, iconst, != )
RakePix	(rm_quad_ge, rd_quad_ge, QuadPixel, iconst, >= )
RakePix	(rm_quad_gt, rd_quad_gt, QuadPixel, iconst, >  )

RakePix	(rm_real_lt, rd_real_lt, RealPixel, fconst, <  )
RakePix	(rm_real_le, rd_real_le, RealPixel, fconst, <= )
RakePix	(rm_real_eq, rd_real_eq, RealPixel, fconst, == )
RakePix	(rm_real_ne, rd_real_ne, RealPixel, fconst, != )
RakePix	(rm_real_gt, rd_real_gt, RealPixel, fconst, >  )
RakePix	(rm_real_ge, rd_real_ge, RealPixel, fconst, >= )

static void (*action_monoROI[5][6])() = {
	rm_real_lt, rm_real_le, rm_real_eq, rm_real_ne, rm_real_gt, rm_real_ge,
	 rm_bit_lt,  rm_bit_le,  rm_bit_eq,  rm_bit_ne,  rm_bit_gt,  rm_bit_ge,
	rm_byte_lt, rm_byte_le, rm_byte_eq, rm_byte_ne, rm_byte_gt, rm_byte_ge,
	rm_pair_lt, rm_pair_le, rm_pair_eq, rm_pair_ne, rm_pair_gt, rm_pair_ge,
	rm_quad_lt, rm_quad_le, rm_quad_eq, rm_quad_ne, rm_quad_gt, rm_quad_ge
};
static void (*action_dyadROI[5][6])() = {
	rd_real_lt, rd_real_le, rd_real_eq, rd_real_ne, rd_real_gt, rd_real_ge,
	rd_bit_lt,  rd_bit_le,  rd_bit_eq,  rd_bit_ne,  rd_bit_gt,  rd_bit_ge,
	rd_byte_lt, rd_byte_le, rd_byte_eq, rd_byte_ne, rd_byte_gt, rd_byte_ge,
	rd_pair_lt, rd_pair_le, rd_pair_eq, rd_pair_ne, rd_pair_gt, rd_pair_ge,
	rd_quad_lt, rd_quad_le, rd_quad_eq, rd_quad_ne, rd_quad_gt, rd_quad_ge
};

/*------------------------------------------------------------------------
----------------  Triple Band Operations (with/without ROI) --------------
------------------------------------------------------------------------*/

/* mono: (*(pvt->action)) (dvoid, src1, pvt, run, ix); */
/* dyad: (*(pvt->action)) (dvoid, src1, src2, run, ix); */

#define tb_name1_body()							\
    CARD32 M, D, px = LOGINDX(x);					\
    LogInt S1, *src = ((LogInt *) src1) + px;				\
    LogInt S2 = (pvt->iconst ? LOGONES : LOGZERO);			\
    dst += px; 								\
    px = x & LOGMASK;							\
    if ((px + dx) >= LOGSIZE) {						\
	if (px) {							\
	    S1 = *src++; 						\
	    D = *dst;							\
	    M = BitRight(LOGONES,px);					\
	    dx -= (LOGSIZE - px);					\
	    *dst++ = D & ~(M & (S1 ^ S2));				\
	}								\
	for (px = dx >> LOGSHIFT; px > 0; px--) {			\
	    S1 = *src++; 						\
	    D = *dst;							\
	    /* turn off (&~) unequal (^) bits */			\
	    *dst++ = D & ~(S1 ^ S2);					\
	}								\
	if (dx &= LOGMASK) {						\
	    S1 = *src; 							\
	    D = *dst;							\
	    M = ~BitRight(LOGONES,dx);					\
	    *dst = D & ~(M & (S1 ^ S2));				\
	}								\
    } else {								\
	    S1 = *src;	 						\
	    D = *dst;							\
	    M = BitRight(LOGONES,px) & ~(BitRight(LOGONES,px+dx));	\
	    *dst = D & ~(M & (S1 ^ S2));				\
    }
/* end of tb_name1_body */

#define tb_name2_body()							\
    CARD32 M, D, px = LOGINDX(x);					\
    LogInt S1, *src = ((LogInt *) src1) + px;				\
    LogInt S2, *trc = ((LogInt *) src2) + px;				\
    dst += px; 								\
    px = x & LOGMASK;							\
    if ((px + dx) >= LOGSIZE) {						\
	if (px) {							\
	    S1 = *src++; S2 = *trc++;					\
	    D = *dst;							\
	    M = BitRight(LOGONES,px);					\
	    dx -= (LOGSIZE - px);					\
	    *dst++ = D & ~(M & (S1 ^ S2));				\
	}								\
	for (px = dx >> LOGSHIFT; px > 0; px--) {			\
	    S1 = *src++; S2 = *trc++;					\
	    D = *dst;							\
	    *dst++ = D & ~(S1 ^ S2);					\
	}								\
	if (dx &= LOGMASK) {						\
	    S1 = *src; S2 = *trc;					\
	    D = *dst;							\
	    M = ~BitRight(LOGONES,dx);					\
	    *dst = D & ~(M & (S1 ^ S2));				\
	}								\
    } else {								\
	    S1 = *src; S2 = *trc;					\
	    D = *dst;							\
	    M = BitRight(LOGONES,px) & ~(BitRight(LOGONES,px+dx));	\
	    *dst = D & ~(M & (S1 ^ S2));				\
    }		
/* end of tb_name2_body */

#define TakeBit(name1, name2)	 					\
static void name1(dst,src1,pvt,dx,x)					\
    LogInt *dst;							\
    pointer src1;							\
    mpComparePvtPtr pvt;						\
    INT32  dx, x;							\
{									\
tb_name1_body()								\
}									\
static void name2(dst,src1,src2,dx,x)					\
    LogInt *dst;							\
    pointer src1, src2;							\
    INT32  dx, x;							\
{									\
tb_name2_body()								\
}

#define TakePix(name1, name2, itype, cnst_name)				\
static void name1(dst,src1,pvt,dx,x)					\
    LogInt *dst;							\
    pointer src1;							\
    mpComparePvtPtr pvt;						\
    INT32  dx, x;							\
{									\
    itype *src = ((itype *) src1) + x;					\
    itype con = (itype) pvt->cnst_name;					\
    LogInt M, value;							\
    dst += LOGINDX(x); 							\
    if (x & LOGMASK) {							\
	for (value = 0, M=LOGBIT(x); dx && M; dx--, LOGRIGHT(M))	\
	    if (*src++ != con)						\
		value |= M;						\
	*dst++ &= ~value;						\
    }									\
    for ( ; dx >= LOGSIZE; dx -= LOGSIZE, dst++) {			\
	/* if (!*dst) { src += LOGSIZE; } else */			\
	for (value = 0, M=LOGLEFT; M ; LOGRIGHT(M))			\
	    if (*src++ != con)						\
		value |= M;						\
	*dst &= ~value;							\
    }									\
    if (dx > 0) {							\
	for (value = 0, M=LOGLEFT; dx; dx--, LOGRIGHT(M))		\
	    if (*src++ != con)						\
		value |= M;						\
	*dst &= ~value;							\
    }									\
}									\
static void name2(dst,src1,src2,dx,x)					\
    LogInt *dst;							\
    pointer src1, src2;							\
    INT32  dx, x;							\
{									\
    itype *src = ((itype *) src1) + x;					\
    itype *trc = ((itype *) src2) + x;					\
    LogInt M, value;							\
    dst += LOGINDX(x); 							\
    if (x & LOGMASK) {							\
	for (value = 0, M=LOGBIT(x); dx && M; dx--, LOGRIGHT(M))	\
	    if (*src++ != *trc++)					\
		value |= M;						\
	*dst++ &= ~value;						\
    }									\
    for ( ; dx >= LOGSIZE; dx -= LOGSIZE, dst++) {			\
	/* if (!*dst) { src += LOGSIZE; trc += LOGSIZE; } else */	\
	for (value = 0, M=LOGLEFT; M ; LOGRIGHT(M))			\
	    if (*src++ != *trc++)					\
		value |= M;						\
	*dst &= ~value;							\
    }									\
    if (dx > 0) {							\
	for (value = 0, M=LOGLEFT; dx; dx--, LOGRIGHT(M))		\
	    if (*src++ != *trc++)					\
		value |= M;						\
	*dst++ &= ~value;						\
    }									\
}

TakeBit	(tm_bit,  td_bit)
TakePix	(tm_byte, td_byte, BytePixel, iconst)
TakePix	(tm_pair, td_pair, PairPixel, iconst)
TakePix	(tm_quad, td_quad, QuadPixel, iconst)
TakePix	(tm_real, td_real, RealPixel, fconst)

static void (*action_mtrip[5])() =
	{ tm_real, tm_bit, tm_byte, tm_pair, tm_quad };

static void (*action_dtrip[5])() =
	{ td_real, td_bit, td_byte, td_pair, td_quad };

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/

static int InitializeCompare(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    peTexPtr 	     pet = ped->peTex;
    xieFloCompare   *raw = (xieFloCompare *) ped->elemRaw;
    pCompareDefPtr  epvt = (pCompareDefPtr)  ped->elemPvt;
    mpComparePvtPtr  pvt = (mpComparePvtPtr) pet->private;
    receptorPtr      rcp = pet->receptor;
    CARD32	  nbands = rcp[SRCt1].inFlo->bands;
    bandPtr	   sband = &(rcp[SRCt1].band[0]);
    bandPtr 	   tband = &(rcp[SRCt2].band[0]);
    bandPtr	   dband = &(pet->emitter[0]);
    CARD8	     msk = raw->bandMask;
    BOOL	  hasROI = raw->domainPhototag != 0;
    BOOL     doingtriple = raw->combine && (nbands == 3);
    CARD32	    band;

    if (doingtriple) {
	if (raw->src2)
	    ped->ddVec.activate = ActivateCompareTripleD;
	else
	    ped->ddVec.activate = ActivateCompareTripleM;
#if defined(NON_ROI)
    } else if (!hasROI) {
	if (raw->src2)
	    ped->ddVec.activate = ActivateCompareD;
	else
	    ped->ddVec.activate = ActivateCompareM;
#endif
    } else {
	if (raw->src2)
	    ped->ddVec.activate = ActivateCompareDROI;
	else
	    ped->ddVec.activate = ActivateCompareMROI;
    }
    for (band=0; band<nbands; band++, pvt++, sband++, tband++, dband++) {
	CARD32 iclass = IndexClass(sband->format->class);
	pvt->width = sband->format->width;
	pvt->final = FALSE;
	if (raw->src2 && (pvt->width > tband->format->width))
	    pvt->width = tband->format->width;
	if (doingtriple) {
	    if (raw->src2)
		pvt->action = action_dtrip[iclass];
	    else
		pvt->action = action_mtrip[iclass];
#if defined(NON_ROI)
	} else if (!hasROI) {
	    if (raw->src2) {
		pvt->action = action_dyad[iclass][raw->operator-1];
		pvt->action2 = (void (*)()) NULL;
		pvt->endix = dband->format->width; /* pixels to do */
		if (dband->format->width > tband->format->width) {
		    pvt->action2 = action_clear;
		    pvt->endix = tband->format->width; /* pixels to do */
		    pvt->endrun = dband->format->width -
				tband->format->width; /* pixels to pad */
		}
	    } else
		pvt->action = action_mono[iclass][raw->operator-1];
#endif
	} else { /* ROI */
	    if (raw->src2)
		pvt->action = action_dyadROI[iclass][raw->operator-1];
	    else
		pvt->action = action_monoROI[iclass][raw->operator-1];
	}
	if (!raw->src2) {
	    double dub = epvt->constant[band];
	    pvt->fconst = dub;
	    pvt->iconst = ConstrainConst(dub, sband->format->levels);
	}
    }

    /* If processing domain, allow replication */
    if (hasROI && !doingtriple)
	rcp[ped->inCnt-1].band[0].replicate = msk;

    InitReceptor(flo, ped, &rcp[SRCt1], NO_DATAMAP, 1, msk, NO_BANDS);

    if (raw->src2)
	InitReceptor(flo, ped, &rcp[SRCt2], NO_DATAMAP, 1, msk, NO_BANDS);

#if defined(NON_ROI)
    if (hasROI || doingtriple)
#endif
	InitProcDomain(flo, ped, raw->domainPhototag, raw->domainOffsetX, 
							raw->domainOffsetY);
    InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE);

    pet->bandSync = doingtriple ? SYNC : NO_SYNC;

    return !ferrCode(flo);
}
	

/* end module mpcomp.c */
