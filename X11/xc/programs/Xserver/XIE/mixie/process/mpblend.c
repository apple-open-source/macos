/* $Xorg: mpblend.c,v 1.4 2001/02/09 02:04:30 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module mpblend.c ****/
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
  
	mpblend.c -- DDXIE Blend element
  
	Dean Verheiden -- AGE Logic, Inc. June, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mpblend.c,v 3.5 2001/12/14 19:58:43 dawes Exp $ */

#define _XIEC_MPBLEND
#define _XIEC_PBLEND

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
#include <memory.h>


typedef float BlendFloat;
/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzeBlend();

/*
 *  routines used internal to this module
 */
static int CreateBlend();
static int ResetBlend();
static int DestroyBlend();

static int InitializeMonoBlend();
static int InitializeDualBlend();
static int InitializeMonoAlphaBlend();
static int InitializeDualAlphaBlend();

static int MonoBlend();
static int DualBlend();
static int MonoAlphaBlend();
static int DualAlphaBlend();


/*
 * DDXIE Blend entry points
 */
static ddElemVecRec BlendVec = {
  CreateBlend,
  (xieIntProc)NULL,
  (xieIntProc)NULL,
  (xieIntProc)NULL,
  ResetBlend,
  DestroyBlend
  };

/*
 * Local Declarations. 
 */

typedef struct _mpblenddef {
	void	(*action) ();
} mpBlendPvtRec, *mpBlendPvtPtr;


/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeBlend(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloBlend *raw = (xieFloBlend *)ped->elemRaw;
  pBlendDefPtr pvt = (pBlendDefPtr)ped->elemPvt;
  CARD16 aindex    = pvt->aindex;
  
  /* for now just stash our entry point vector in the peDef */
  ped->ddVec = BlendVec;
  if (aindex) {
	if (raw->src2) {
  		ped->ddVec.initialize = InitializeDualAlphaBlend;
  		ped->ddVec.activate   = DualAlphaBlend;
	} else {
  		ped->ddVec.initialize = InitializeMonoAlphaBlend;
  		ped->ddVec.activate   = MonoAlphaBlend;
	}
  } else {
	if (raw->src2) {
  		ped->ddVec.initialize = InitializeDualBlend;
  		ped->ddVec.activate   = DualBlend;
	} else {
  		ped->ddVec.initialize = InitializeMonoBlend;
  		ped->ddVec.activate   = MonoBlend;
	}
  }

  return(TRUE);
}                               /* end miAnalyzeBlend */

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateBlend(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  int auxsize = xieValMaxBands * sizeof(mpBlendPvtRec);

  /* always force syncing between inputs (is nop if only one input) */
  return MakePETex(flo, ped, auxsize, SYNC , NO_SYNC);
}                               /* end CreateBlend */


static void MonoR(), MonoQ(), MonoP(), MonoB();
/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeMonoBlend(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloBlend  *raw = (xieFloBlend *)ped->elemRaw;
  CARD8         msk = raw->bandMask;
  peTexPtr      pet = ped->peTex;
  receptorPtr   rcp = pet->receptor;
  mpBlendPvtPtr pvt = (mpBlendPvtPtr) pet->private;
  int band, nbands;
  bandPtr iband;

   /* If processing domain, allow replication */
  if (raw->domainPhototag)
      pet->receptor[ped->inCnt-1].band[0].replicate = msk;

  if (!(InitReceptor(flo,ped,&rcp[SRCt1],NO_DATAMAP,1,msk,~msk) && 
        InitProcDomain(flo, ped, raw->domainPhototag, raw->domainOffsetX, 
				raw->domainOffsetY) &&
        InitEmitter(flo,ped,NO_DATAMAP,SRCt1)))
	return (FALSE);
 
  /* Figure out the appropriate action vector */
  nbands = pet->receptor[SRCtag].inFlo->bands;
  iband = &(pet->receptor[SRCtag].band[0]);
  for(band = 0; band < nbands; band++, pvt++, iband++) {
      switch (iband->format->class) {
      case UNCONSTRAINED:     pvt->action = MonoR; break;
      case QUAD_PIXEL:        pvt->action = MonoQ; break;
      case PAIR_PIXEL:        pvt->action = MonoP; break;
      case BYTE_PIXEL:        pvt->action = MonoB; break;
      case BIT_PIXEL:         
      default:                ImplementationError(flo, ped, return(FALSE));
                              break;
      }
  }

  return( TRUE );
}                               /* end InitializeMonoBlend */

static void DualR(), DualQ(), DualP(), DualB();
/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeDualBlend(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloBlend *raw  = (xieFloBlend *)ped->elemRaw;
  peTexPtr pet	    = ped->peTex;
  CARD8         msk = raw->bandMask;
  receptorPtr   rcp = pet->receptor;
  mpBlendPvtPtr pvt = (mpBlendPvtPtr) pet->private;
  int band, nbands;
  bandPtr iband;

   /* If processing domain, allow replication */
  if (raw->domainPhototag)
      pet->receptor[ped->inCnt-1].band[0].replicate = msk;

  if (!(InitReceptor(flo,ped,&rcp[SRCt1],NO_DATAMAP,1,msk,~msk) && 
        InitReceptor(flo,ped,&rcp[SRCt2],NO_DATAMAP,1,msk,NO_BANDS) && 
	InitProcDomain(flo, ped, raw->domainPhototag, raw->domainOffsetX, 
					raw->domainOffsetY) &&
        InitEmitter(flo,ped,0,SRCt1)))
	return (FALSE);

  /* Figure out the appropriate action vector */
  nbands = pet->receptor[SRCtag].inFlo->bands;
  iband = &(pet->receptor[SRCtag].band[0]);
  for(band = 0; band < nbands; band++, pvt++, iband++) {
      switch (iband->format->class) {
      case UNCONSTRAINED:     pvt->action = DualR; break;
      case QUAD_PIXEL:        pvt->action = DualQ; break;
      case PAIR_PIXEL:        pvt->action = DualP; break;
      case BYTE_PIXEL:        pvt->action = DualB; break;
      case BIT_PIXEL:         
      default:                ImplementationError(flo, ped, return(FALSE));
                              break;
      }
  }

  return( TRUE );
}                               /* end InitializeDualBlend */

static void MonoAlphaRQ(), MonoAlphaQQ(), MonoAlphaPQ(), MonoAlphaBQ();
static void MonoAlphaRP(), MonoAlphaQP(), MonoAlphaPP(), MonoAlphaBP();
static void MonoAlphaRB(), MonoAlphaQB(), MonoAlphaPB(), MonoAlphaBB();
/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeMonoAlphaBlend(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  peTexPtr      pet = ped->peTex;
  xieFloBlend  *raw = (xieFloBlend *)ped->elemRaw;
  CARD8         msk = raw->bandMask;
  receptorPtr   rcp = pet->receptor;
  CARD16     aindex = ((pBlendDefPtr)ped->elemPvt)->aindex;
  bandPtr        ab = &pet->receptor[aindex].band[0];
  CARD8      nbands = pet->receptor[SRCtag].inFlo->bands;
  CARD8      abands = pet->receptor[aindex].inFlo->bands;
  mpBlendPvtPtr pvt = (mpBlendPvtPtr) pet->private;
  int band;
  bandPtr iband;

  /* Replicate the alpha plane to all active bands */
  if (nbands == 3 && abands == 1)
      ab->replicate = msk;

   /* If processing domain, allow replication */
  if (raw->domainPhototag)
      pet->receptor[ped->inCnt-1].band[0].replicate = msk;

  if (!(InitReceptor(flo,ped,&rcp[SRCt1],NO_DATAMAP,1,msk,~msk) && 
        InitReceptor(flo,ped,&rcp[aindex],NO_DATAMAP,1,1,NO_BANDS) && 
	InitProcDomain(flo, ped, raw->domainPhototag, raw->domainOffsetX, 
					raw->domainOffsetY) &&
        InitEmitter(flo,ped,0,SRCt1)))
	return (FALSE);

  /* Figure out the appropriate action vector based on src and alpha plane */
  iband = &(pet->receptor[SRCtag].band[0]);
  for(band = 0; band < nbands; band++, pvt++, iband++) {
      switch (iband->format->class) {
      case UNCONSTRAINED:     
          switch (ab->format->class) {
      	      case QUAD_PIXEL:        pvt->action = MonoAlphaRQ; break;
	      case PAIR_PIXEL:        pvt->action = MonoAlphaRP; break;
              case BYTE_PIXEL:        pvt->action = MonoAlphaRB; break;
              case BIT_PIXEL:         
      	      default:                ImplementationError(flo, ped, 
							return(FALSE));
                                      break;
          }
	  break;
      case QUAD_PIXEL:       
          switch (ab->format->class) {
      	      case QUAD_PIXEL:        pvt->action = MonoAlphaQQ; break;
	      case PAIR_PIXEL:        pvt->action = MonoAlphaQP; break;
              case BYTE_PIXEL:        pvt->action = MonoAlphaQB; break;
              case BIT_PIXEL:         
      	      default:                ImplementationError(flo, ped, 
							return(FALSE));
                                      break;
          }
	  break;
      case PAIR_PIXEL:        
          switch (ab->format->class) {
      	      case QUAD_PIXEL:        pvt->action = MonoAlphaPQ; break;
	      case PAIR_PIXEL:        pvt->action = MonoAlphaPP; break;
              case BYTE_PIXEL:        pvt->action = MonoAlphaPB; break;
              case BIT_PIXEL:         
      	      default:                ImplementationError(flo, ped, 
							return(FALSE));
                                      break;
          }
	  break;
      case BYTE_PIXEL:        
          switch (ab->format->class) {
      	      case QUAD_PIXEL:        pvt->action = MonoAlphaBQ; break;
	      case PAIR_PIXEL:        pvt->action = MonoAlphaBP; break;
              case BYTE_PIXEL:        pvt->action = MonoAlphaBB; break;
              case BIT_PIXEL:         
      	      default:                ImplementationError(flo, ped, 
							return(FALSE));
                                      break;
          }
	  break;
      case BIT_PIXEL:         
      default:                ImplementationError(flo, ped, return(FALSE));
                              break;
      }
  }

  return( TRUE );
}                               /* end InitializeMonoAlphaBlend */

static void DualAlphaRQ(), DualAlphaQQ(), DualAlphaPQ(), DualAlphaBQ();
static void DualAlphaRP(), DualAlphaQP(), DualAlphaPP(), DualAlphaBP();
static void DualAlphaRB(), DualAlphaQB(), DualAlphaPB(), DualAlphaBB();
/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeDualAlphaBlend(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  peTexPtr      pet = ped->peTex;
  xieFloBlend  *raw = (xieFloBlend *)ped->elemRaw;
  CARD8         msk = raw->bandMask;
  receptorPtr   rcp = pet->receptor;
  CARD16     aindex = ((pBlendDefPtr)ped->elemPvt)->aindex;
  bandPtr        ab = &pet->receptor[aindex].band[0];
  mpBlendPvtPtr pvt = (mpBlendPvtPtr) pet->private;
  CARD8      nbands = pet->receptor[SRCtag].inFlo->bands;
  CARD8      abands = pet->receptor[aindex].inFlo->bands;
  int band;
  bandPtr iband;

  /* Replicate the alpha plane to all active bands */
  if (nbands == 3 && abands == 1)
      ab->replicate = msk;

   /* If processing domain, allow replication */
  if (raw->domainPhototag)
      pet->receptor[ped->inCnt-1].band[0].replicate = msk;

  if (!(InitReceptor(flo,ped,&rcp[SRCt1],NO_DATAMAP,1,msk,~msk) && 
        InitReceptor(flo,ped,&rcp[SRCt2],NO_DATAMAP,1,msk,NO_BANDS) && 
        InitReceptor(flo,ped,&rcp[aindex],NO_DATAMAP,1,1,NO_BANDS) && 
	InitProcDomain(flo, ped, raw->domainPhototag, raw->domainOffsetX, 
				raw->domainOffsetY) &&
        InitEmitter(flo,ped,0,SRCt1)))
	return (FALSE);

  /* Figure out the appropriate action vector based on src and alpha plane */
  iband = &(pet->receptor[SRCtag].band[0]);
  for(band = 0; band < nbands; band++, pvt++, iband++) {
      switch (iband->format->class) {
      case UNCONSTRAINED:     
          switch (ab->format->class) {
      	      case QUAD_PIXEL:        pvt->action = DualAlphaRQ; break;
	      case PAIR_PIXEL:        pvt->action = DualAlphaRP; break;
              case BYTE_PIXEL:        pvt->action = DualAlphaRB; break;
              case BIT_PIXEL:         
      	      default:                ImplementationError(flo, ped, 
							return(FALSE));
                                      break;
          }
	  break;
      case QUAD_PIXEL:       
          switch (ab->format->class) {
      	      case QUAD_PIXEL:        pvt->action = DualAlphaQQ; break;
	      case PAIR_PIXEL:        pvt->action = DualAlphaQP; break;
              case BYTE_PIXEL:        pvt->action = DualAlphaQB; break;
              case BIT_PIXEL:         
      	      default:                ImplementationError(flo, ped, 
							return(FALSE));
                                      break;
          }
	  break;
      case PAIR_PIXEL:        
          switch (ab->format->class) {
      	      case QUAD_PIXEL:        pvt->action = DualAlphaPQ; break;
	      case PAIR_PIXEL:        pvt->action = DualAlphaPP; break;
              case BYTE_PIXEL:        pvt->action = DualAlphaPB; break;
              case BIT_PIXEL:         
      	      default:                ImplementationError(flo, ped, 
							return(FALSE));
                                      break;
          }
	  break;
      case BYTE_PIXEL:        
          switch (ab->format->class) {
      	      case QUAD_PIXEL:        pvt->action = DualAlphaBQ; break;
	      case PAIR_PIXEL:        pvt->action = DualAlphaBP; break;
              case BYTE_PIXEL:        pvt->action = DualAlphaBB; break;
              case BIT_PIXEL:         
      	      default:                ImplementationError(flo, ped, 
							return(FALSE));
                                      break;
          }
	  break;
      case BIT_PIXEL:         
      default:                ImplementationError(flo, ped, return(FALSE));
                              break;
      }
  }

  return( TRUE );
}                               /* end InitializeDualAlphaBlend */

/*------------------------------------------------------------------------
------------------------ crank some single input data --------------------
------------------------------------------------------------------------*/
static int MonoBlend(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  pBlendDefPtr pvt   = (pBlendDefPtr) ped->elemPvt;
  BlendFloat aconst1 = pvt->alphaConst;
  BlendFloat aconst2 = (BlendFloat)1.0 - aconst1;
  double *sconst     = pvt->constant;
  receptorPtr rcp    = pet->receptor;
  CARD32 bands       = rcp[SRCt1].inFlo->bands;
  bandPtr sb1        = &rcp[SRCt1].band[0];
  bandPtr bnd        = &pet->emitter[0];
  mpBlendPvtPtr mpvt = (mpBlendPvtPtr) pet->private;
  pointer sr1, dst;
  CARD32 b;
  
  for(b = 0; b < bands; b++, sb1++, bnd++, sconst++, mpvt++) {
    BlendFloat offset = *sconst * aconst1;

    if (!(pet->scheduled & 1<<b)) continue;
    
    /* get pointers to the initial src and dst scanlines */
    sr1 = GetCurrentSrc(flo,pet,sb1);
    dst = GetCurrentDst(flo,pet,bnd);

    /* continue while all is well and we have pointers */
    while(!ferrCode(flo) && sr1 && dst && 
				SyncDomain(flo,ped,bnd,FLUSH)) {
      INT32  run, currentx = 0;

      if (sr1 != dst) memcpy (dst, sr1, bnd->pitch);
      
      while (run = GetRun(flo,pet,bnd)) {
	if (run > 0) {
      	    (*(mpvt->action)) (currentx,run,sr1,dst,aconst2,offset);
	    currentx += run;
	} else 
	    currentx -= run;
      }

      /* get pointers to the next src and dst scanlines */
      sr1 = GetNextSrc(flo,pet,sb1,TRUE);
      dst = GetNextDst(flo,pet,bnd,TRUE);
    }
    /* make sure the scheduler knows how much src we used */
    FreeData(flo,pet,sb1,sb1->current);
  }
  return(TRUE);
}                               /* end MonoBlend */

/*------------------------------------------------------------------------
-- Case with two inputs: src1, src2, no alpha plane ---------------------
------------------------------------------------------------------------*/
static int DualBlend(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  BlendFloat aconst1 = ((pBlendDefPtr)ped->elemPvt)->alphaConst;
  BlendFloat aconst2 = 1.0 - aconst1;
  receptorPtr  rcp   = pet->receptor;
  CARD32 bands       = rcp[SRCt1].inFlo->bands;
  bandPtr sb1        = &rcp[SRCt1].band[0];
  bandPtr sb2        = &rcp[SRCt2].band[0];
  bandPtr bnd        = &pet->emitter[0];
  mpBlendPvtPtr mpvt = (mpBlendPvtPtr) pet->private;
  pointer sr1, sr2, dst;
  CARD32 b, w;
  
  for(b = 0; b < bands; b++,sb1++,sb2++,bnd++,mpvt++) {

    if (!(pet->scheduled & 1<<b)) continue;
    
    /* Figure out if any data from the first band passes through unchanged */
    if(sb1->format->width > sb2->format->width) 
	w = sb2->format->width;
    else 
        w = sb1->format->width;

    /* get pointers to the initial src-1, src-2, and dst scanlines */
    sr1 = GetCurrentSrc(flo,pet,sb1);
    sr2 = GetCurrentSrc(flo,pet,sb2);
    dst = GetCurrentDst(flo,pet,bnd);
	
    /* continue while all is well and we have pointers */
    while(!ferrCode(flo) && sr1 && sr2 && dst &&
				SyncDomain(flo,ped,bnd,FLUSH)) {
      INT32  run, currentx = 0;

      if (sr1 != dst) memcpy (dst, sr1, bnd->pitch);
      
      while ((run = GetRun(flo,pet,bnd)) && currentx < w) {
	if (run > 0) {
	    if (currentx + run > w) 	/* Yuck, have to clip */
		run = w - currentx;
            (*(mpvt->action)) (currentx,run,sr1,sr2,dst,aconst1,aconst2);
	    currentx += run;
	} else 
	    currentx -= run;
      }

      /* get pointers to the next src-1, src-2, and dst scanlines */
      sr1 = GetNextSrc(flo,pet,sb1,TRUE);
      sr2 = GetNextSrc(flo,pet,sb2,TRUE);
      dst = GetNextDst(flo,pet,bnd,TRUE);
    }

    /* If src2 < sr1, pass remaining lines through untouched */
    if(!sr1 && sb1->final)		/* when sr1 runs out, we're done    */
      DisableSrc(flo,pet,sb2,FLUSH);
    else if(!sr2 && sb2->final)		/* when sr2 runs out, pass-thru sr1 */
      BypassSrc(flo,pet,sb1);
    else { 	/* both inputs still active, keep the scheduler up to date  */
      FreeData(flo,pet,sb1,sb1->current);
      FreeData(flo,pet,sb2,sb2->current);
    }
  }
  return(TRUE);
}                               /* end DualBlend */

/*------------------------------------------------------------------------
-- Case with two inputs: src1, alpha plane, no src2  ---------------------
------------------------------------------------------------------------*/
static int MonoAlphaBlend(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  pBlendDefPtr pvt     = (pBlendDefPtr) ped->elemPvt;
  BlendFloat invaconst = 1/pvt->alphaConst;
  CARD16 aindex        = pvt->aindex;
  receptorPtr rcp      = pet->receptor;
  CARD32  bands        = rcp[SRCt1].inFlo->bands;
  bandPtr sb1          = &rcp[SRCt1].band[0];
  bandPtr aband        = &rcp[aindex].band[0];
  bandPtr bnd          = &pet->emitter[0];
  double *sconst       = pvt->constant;
  mpBlendPvtPtr mpvt   = (mpBlendPvtPtr) pet->private;
  pointer sr1, alpha, dst;
  CARD32 b, w;
  
  for(b = 0; b < bands; b++,sb1++,bnd++,sconst++, mpvt++, aband++) {
    BlendFloat scalefactor = *sconst * invaconst;

    if (!(pet->scheduled & 1<<b)) continue;
    
    /* Figure out if any data from the first band passes through unchanged */
    if(sb1->format->width > aband->format->width) 
	w = aband->format->width;
    else 
        w = sb1->format->width;

    /* get pointers to the initial src-1, alpha, and dst scanlines */
    sr1   = GetCurrentSrc(flo,pet,sb1);
    alpha = GetCurrentSrc(flo,pet,aband);
    dst   = GetCurrentDst(flo,pet,bnd);
	
    /* continue while all is well and we have pointers */
    while(!ferrCode(flo) && sr1 && alpha && dst &&
				SyncDomain(flo,ped,bnd,FLUSH)) {
      INT32  run, currentx = 0;

      if (sr1 != dst) memcpy (dst, sr1, bnd->pitch);
      
      while ((run = GetRun(flo,pet,bnd)) && currentx < w ) {
	if (run > 0) {
	    if (currentx + run > w ) 	/* Yuck, have to clip */
		run = w - currentx;
            (*(mpvt->action))(currentx,run,sr1,alpha,dst,invaconst,scalefactor);
	    currentx += run;
	} else 
	    currentx -= run;
      }

      /* get pointers to the next src-1 and dst scanlines */
      sr1   = GetNextSrc(flo,pet,sb1,FLUSH);
      alpha = GetNextSrc(flo,pet,aband,FLUSH);
      dst   = GetNextDst(flo,pet,bnd,FLUSH);
    }

    /* If alpha < sr1, pass remaining lines through untouched */
    if(!sr1 && sb1->final) {		/* when sr1 runs out, we're done    */
    } else if(!alpha && aband->final) {	/* when alpha runs out, pass-thru sr1*/
      pet->inSync = FALSE;		/* No need to sync anymore	    */
      BypassSrc(flo,pet,sb1);
    } else { 	/* both inputs still active, keep the scheduler up to date  */
      FreeData(flo,pet,sb1,sb1->current);
      FreeData(flo,pet,aband,aband->current);
    }
  }

  return(TRUE);
}                               /* end MonoAlphaBlend */

/*------------------------------------------------------------------------
-- Case with three inputs: src1, src2, and alpha plane -------------------
------------------------------------------------------------------------*/
static int DualAlphaBlend(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  pBlendDefPtr pvt     = (pBlendDefPtr)ped->elemPvt;
  BlendFloat invaconst = 1.0/pvt->alphaConst;
  CARD16 aindex        = pvt->aindex;
  receptorPtr rcp      = pet->receptor;
  CARD32 bands         = rcp[SRCt1].inFlo->bands;
  bandPtr sb1          = &rcp[SRCt1].band[0];
  bandPtr sb2          = &rcp[SRCt2].band[0];
  bandPtr aband        = &rcp[aindex].band[0];
  bandPtr bnd          = &pet->emitter[0];
  mpBlendPvtPtr mpvt   = (mpBlendPvtPtr) pet->private;
  pointer sr1, sr2, alpha, dst;
  CARD32 b, w;

  for(b = 0; b < bands; b++,sb1++,sb2++,bnd++,mpvt++,aband++) {

    if (!(pet->scheduled & 1<<b)) continue;
    
    /* Figure out if any data from the first band passes through unchanged */
    /* Pass through if either sb2 or alpha runs out 			   */
    if(sb1->format->width > sb2->format->width ||
		  sb1->format->width > aband->format->width) 
	w = (aband->format->width < sb2->format->width) ?
			aband->format->width : sb2->format->width;
    else 
        w = sb1->format->width;

    /* get pointers to the initial src-1, src-2, and dst scanlines */
    sr1   = GetCurrentSrc(flo,pet,sb1);
    sr2   = GetCurrentSrc(flo,pet,sb2);
    alpha = GetCurrentSrc(flo,pet,aband);
    dst   = GetCurrentDst(flo,pet,bnd);
	
    /* continue while all is well and we have pointers */
    while(!ferrCode(flo) && sr1 && sr2 && alpha && dst &&
				SyncDomain(flo,ped,bnd,FLUSH)) {
      INT32  run, currentx = 0;

      if (sr1 != dst) memcpy (dst, sr1, bnd->pitch);
      
      while ((run = GetRun(flo,pet,bnd)) && currentx < w ) {
	if (run > 0) {
	    if (currentx + run > w) 	/* Yuck, have to clip */
		run = w - currentx;
            (*(mpvt->action)) (currentx,run,sr1,sr2,alpha,dst,invaconst);
	    currentx += run;
	} else 
	    currentx -= run;
      }

      /* get pointers to the next src-1, src-2, and dst scanlines */
      sr1   = GetNextSrc(flo,pet,sb1,FLUSH);
      sr2   = GetNextSrc(flo,pet,sb2,FLUSH);
      alpha = GetNextSrc(flo,pet,aband,FLUSH);
      dst   = GetNextDst(flo,pet,bnd,FLUSH);
    }

    /* If alpha < sr1, pass remaining lines through untouched */
    if(!sr1 && sb1->final) {		/* when sr1 runs out, we're done    */
      DisableSrc(flo,pet,sb2,FLUSH);
    } else if( !sr2 && sb2->final ||	/* when other inputs out, pass sr1 */
	       !alpha && aband->final ) {	
      pet->inSync = FALSE;		/* No need to sync anymore	   */
      if (sr2) {			/* flush any remain sr2    	   */
	DisableSrc(flo,pet,sb2,FLUSH);
      }
      BypassSrc(flo,pet,sb1);
    } else { 	/* both inputs still active, keep the scheduler up to date  */
      FreeData(flo,pet,sb1,sb1->current);
      FreeData(flo,pet,sb2,sb2->current);
      FreeData(flo,pet,aband,aband->current);
    }
  }

  return(TRUE);
}				/* end DualAlphaBlend */


/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetBlend(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  ResetReceptors(ped);
  ResetProcDomain(ped);
  ResetEmitter(ped);
  
  return(TRUE);
}                               /* end ResetBlend */


/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyBlend(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* get rid of the peTex structure  */
  if(ped->peTex)
    ped->peTex = (peTexPtr) XieFree(ped->peTex);
  
  /* zap this element's entry point vector */
  ped->ddVec.create     = (xieIntProc)NULL;
  ped->ddVec.initialize = (xieIntProc)NULL;
  ped->ddVec.activate   = (xieIntProc)NULL;
  ped->ddVec.reset      = (xieIntProc)NULL;
  ped->ddVec.destroy    = (xieIntProc)NULL;
  
  return(TRUE);
}                               /* end DestroyBlend */

/*------------------------------------------------------------------------
------------------------MonoBlend action routines  ----------------------
------------------------------------------------------------------------*/

#define DOMono(fname,stype)					\
static void fname(count,width,is,id,aconst2,offset) 		\
INT32 count, width;						\
pointer is, id;							\
BlendFloat aconst2, offset;					\
{								\
stype *sr1 = ((stype *)is) + count;				\
stype *dst = ((stype *)id) + count;				\
int i;								\
	for(i = 0; i < width; ++i)				\
		*dst++ = *sr1++ * aconst2 + offset;		\
}

DOMono(MonoR,RealPixel)
DOMono(MonoQ,QuadPixel)
DOMono(MonoP,PairPixel)
DOMono(MonoB,BytePixel)

/*------------------------------------------------------------------------
------------------------DualBlend action routines  ----------------------
------------------------------------------------------------------------*/
#define DODual(fname,stype)					\
static void fname(count,width,is1,is2,id,aconst1,aconst2)	\
INT32 count,width;						\
pointer is1, is2, id;						\
BlendFloat aconst1, aconst2;					\
{								\
stype *sr1 = ((stype *)is1) + count;				\
stype *sr2 = ((stype *)is2) + count;				\
stype *dst = ((stype *)id) + count;				\
int i;								\
      for(i = 0; i < width; ++i)				\
	*dst++ = *sr1++ * aconst2 + *sr2++ * aconst1;		\
}

DODual(DualR,RealPixel)
DODual(DualQ,QuadPixel)
DODual(DualP,PairPixel)
DODual(DualB,BytePixel)

/*------------------------------------------------------------------------
--------------------  MonoAlphaBlend action routines  --------------------
------------------------------------------------------------------------*/
#define DOMonoAlpha(fname,stype,atype)				\
static void fname(count,width,is1,ia,id,iac,sf)			\
INT32 count, width;						\
pointer is1, ia, id;						\
BlendFloat iac, sf;						\
{								\
stype *sr1 = ((stype *)is1) + count;				\
stype *dst = ((stype *)id) + count;				\
atype *alpha = ((atype *)ia) + count;				\
int i;								\
      for(i = 0; i < width; ++i) {				\
	register BlendFloat ralpha = *alpha++;			\
	*dst++ = *sr1++ * ((BlendFloat)1.0 - ralpha * iac) + 	\
					     ralpha * sf;	\
      }								\
}

DOMonoAlpha(MonoAlphaRQ,RealPixel,QuadPixel)
DOMonoAlpha(MonoAlphaRP,RealPixel,PairPixel)
DOMonoAlpha(MonoAlphaRB,RealPixel,BytePixel)
DOMonoAlpha(MonoAlphaQQ,QuadPixel,QuadPixel)
DOMonoAlpha(MonoAlphaQP,QuadPixel,PairPixel)
DOMonoAlpha(MonoAlphaQB,QuadPixel,BytePixel)
DOMonoAlpha(MonoAlphaPQ,PairPixel,QuadPixel)
DOMonoAlpha(MonoAlphaPP,PairPixel,PairPixel)
DOMonoAlpha(MonoAlphaPB,PairPixel,BytePixel)
DOMonoAlpha(MonoAlphaBQ,BytePixel,QuadPixel)
DOMonoAlpha(MonoAlphaBP,BytePixel,PairPixel)
DOMonoAlpha(MonoAlphaBB,BytePixel,BytePixel)

/*------------------------------------------------------------------------
--------------------  DualAlphaBlend action routines  --------------------
------------------------------------------------------------------------*/
#define DODualAlpha(fname,stype,atype)				\
static void fname(count,width,is1,is2,ia,id,iac)		\
CARD32 count,width;						\
pointer is1, is2, ia, id;					\
BlendFloat iac;							\
{								\
stype *sr1   = ((stype *)is1) + count;				\
stype *sr2   = ((stype *)is2) + count;				\
atype *alpha = ((atype *)ia) + count;				\
stype *dst   = ((stype *)id) + count;				\
int i;								\
      for(i = 0; i < width; ++i) {				\
	register BlendFloat ascale = *alpha++ * iac;      	\
	*dst++ = *sr1++ * ((BlendFloat)1.0 - ascale) + 		\
					*sr2++ * ascale;	\
      }							 	\
}

DODualAlpha(DualAlphaRQ,RealPixel,QuadPixel)
DODualAlpha(DualAlphaRP,RealPixel,PairPixel)
DODualAlpha(DualAlphaRB,RealPixel,BytePixel)
DODualAlpha(DualAlphaQQ,QuadPixel,QuadPixel)
DODualAlpha(DualAlphaQP,QuadPixel,PairPixel)
DODualAlpha(DualAlphaQB,QuadPixel,BytePixel)
DODualAlpha(DualAlphaPQ,PairPixel,QuadPixel)
DODualAlpha(DualAlphaPP,PairPixel,PairPixel)
DODualAlpha(DualAlphaPB,PairPixel,BytePixel)
DODualAlpha(DualAlphaBQ,BytePixel,QuadPixel)
DODualAlpha(DualAlphaBP,BytePixel,PairPixel)
DODualAlpha(DualAlphaBB,BytePixel,BytePixel)
/* end module mpblend.c */
