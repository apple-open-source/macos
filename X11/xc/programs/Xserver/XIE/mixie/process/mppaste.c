/* $Xorg: mppaste.c,v 1.4 2001/02/09 02:04:31 xorgcvs Exp $ */
/**** module mppaste.c ****/
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
  
	mppaste.c -- DDXIE paste up element
  
	Dean Verheiden, Larry Hare && Robert NC Shelley -- AGE Logic, Inc. 
							   July, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mppaste.c,v 3.5 2001/12/14 19:58:45 dawes Exp $ */

#define _XIEC_MPPASTE
#define _XIEC_PPASTE

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
int	miAnalyzePasteUp();

/*
 *  routines used internal to this module
 */
static int CreatePasteUp();
static int InitializePasteUp();
static int ActivatePasteUp();
static int ResetPasteUp();
static int DestroyPasteUp();

/* Fill functions */
static void FillReal();
static void FillQuad();
static void FillPair(); 
static void FillByte();
static void FillBit();

/* Action functions */
static void PasteReal();
static void PasteQuad();
static void PastePair(); 
static void PasteByte();
static void PasteBit();

/*
 * DDXIE Paste Up entry points
 */
static ddElemVecRec PasteUpVec = {
  CreatePasteUp,
  InitializePasteUp,
  ActivatePasteUp,
  (xieIntProc)NULL,
  ResetPasteUp,
  DestroyPasteUp
  };

/*
 * Local Declarations. 
 */

typedef struct _pasterect {
    Bool   active;	/* indicates this src is not finished yet  */
    INT32  sxoff;	/* src x offset */
    INT32  dxoff;	/* dst x offset */
    INT32  dyoff;	/* dst y offset */
    CARD32 width;	/* clipped width  of src on output window */
    CARD32 height;	/* clipped height of src on output window */
    CARD32 receptorIndex;
} PasteRectRec, *PasteRectPtr;

typedef struct _mppasteupdef {
	void	     (*fill)();
	void	     (*action)();
	INT32	     nextline;
	CARD32       iconstant;
	CARD32	     numRects;	/* Number of interesting src for this band */
	PasteRectPtr rects;
} mpPasteUpPvtRec, *mpPasteUpPvtPtr;


/*------------------------------------------------------------------------
----------------------------- Analyze ------------------------------------
------------------------------------------------------------------------*/
int miAnalyzePasteUp(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* for now just stash our entry point vector in the peDef */
  ped->ddVec = PasteUpVec;

  return(TRUE);
}                               /* end miAnalyzePasteUp */

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreatePasteUp(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  int auxsize = xieValMaxBands * sizeof(mpPasteUpPvtRec);

  /* Force syncing between sources, but not bands */
  return (MakePETex(flo, ped, auxsize, SYNC, NO_SYNC) );
}                               /* end CreatePasteUp */


/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializePasteUp(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloPasteUp *raw   = (xieFloPasteUp *)ped->elemRaw;
  peTexPtr pet	       = ped->peTex;
  CARD32 nbands	       = pet->receptor[SRCt1].inFlo->bands;
  PasteUpFloat *fconst = ((pPasteUpDefPtr)ped->elemPvt)->constant;
  mpPasteUpPvtPtr pvt;
  xieTypTile *tp;
  bandPtr iband, oband;		
  CARD32 b, t;

  if (!(InitReceptors(flo, ped, NO_DATAMAP, 1) && 
	InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE)))
    return (FALSE);

  /* Figure out the appropriate action vector */
  /* Use first source for the band types since they all must be the same */
  iband  = &(pet->receptor[SRCt1].band[0]);
  pvt = (mpPasteUpPvtPtr)pet->private;
  for(b = 0; b < nbands; b++, pvt++, iband++, fconst++) {

      /* Zero out "interesting" rectangle counters */
      pvt->numRects = 0;
      pvt->nextline = pet->emitter[b].format->height - 1;

      /* Clip fill for constrained data */
      if (IsConstrained(iband->format->class))
          pvt->iconstant = ConstrainConst(*fconst,iband->format->levels);

      switch (iband->format->class) {
      case UNCONSTRAINED:     
		pvt->fill   = FillReal; 
		pvt->action = PasteReal; 
		break;
      case QUAD_PIXEL:        
		pvt->fill   = FillQuad; 
		pvt->action = PasteQuad; 
		break;
      case PAIR_PIXEL:        
		pvt->fill   = FillPair; 
		pvt->action = PastePair; 
		break;
      case BYTE_PIXEL:        
		pvt->fill   = FillByte; 
		pvt->action = PasteByte; 
		break;
      case BIT_PIXEL:         
		pvt->fill   = FillBit; 
		pvt->action = PasteBit; 
		break;
      default:  
		ImplementationError(flo, ped, return(FALSE));
                break;
      }
  }

  /* Disable completely clipped srcs, determine minimum y of non clipped src */
  tp = (xieTypTile *)&(raw[1]);
  for (t = 0; t < raw->numTiles; t++, tp++)  {
	iband = &(pet->receptor[t].band[0]);
	oband = &pet->emitter[0];
  	pvt   = (mpPasteUpPvtPtr)pet->private;
	for(b = 0; b < nbands; b++, iband++, oband++, pvt++) {
  	    INT32 dst_width  = oband->format->width;
  	    INT32 dst_height = oband->format->height;
	    if ((tp->dstY +  (INT32)iband->format->height) <= 0 ||
	        (tp->dstX +  (INT32)iband->format->width)  <= 0 ||
	        (tp->dstX >= dst_width) 	  		||
	        (tp->dstY >= dst_height)) 
		    DisableSrc(flo,pet,iband,FLUSH);
	    else {
	        pvt->numRects++;
		if (tp->dstY < pvt->nextline) 
			pvt->nextline = (tp->dstY > 0) ? tp->dstY : 0;
	    }
	}
  }

  /* Allocate space for srcs that are not completely clipped */
  pvt = (mpPasteUpPvtPtr)pet->private;
  for (b = 0; b < nbands; b++, pvt++) 
	if (pvt->numRects) {
	    pvt->rects = 
		(PasteRectPtr)XieMalloc(pvt->numRects * sizeof(PasteRectRec));
	    pvt->numRects = 0; /* Will be used as list is built */
	} else
	    pvt->rects = (PasteRectPtr)NULL;

  /* 
     Build list for each band containing srcs that are displayed, adjust 
     thresholds for srcs without minimum y 
  */
  tp = (xieTypTile *)&(raw[1]);
  for (t = 0; t < raw->numTiles; t++, tp++)  {
	CARD8 amask = 1, active = pet->receptor[t].active;
	iband = &(pet->receptor[t].band[0]);
	oband = &pet->emitter[0];
  	pvt   = (mpPasteUpPvtPtr)pet->private;
	for(b = 0; b < nbands; b++, iband++, oband++, amask <<= 1, pvt++) {
	    if (active & amask) {
  	        INT32 dst_width  = oband->format->width;
  	        INT32 dst_height = oband->format->height;
		PasteRectPtr pr  = &(pvt->rects[pvt->numRects++]); /* Yuck */

		pr->active = TRUE;
		pr->receptorIndex = t;
		if (tp->dstX >= 0) {			/* clip left */
		    pr->sxoff = 0;
		    pr->dxoff = tp->dstX;
		    pr->width = iband->format->width;
    		} else {
		    pr->sxoff = -tp->dstX;
		    pr->dxoff = 0;
		    pr->width = iband->format->width - pr->sxoff;
		}
		if (pr->dxoff + pr->width > dst_width)	/* clip right */
		    pr->width = dst_width - pr->dxoff;

		if (tp->dstY >= 0) {			/* clip top */
		    pr->dyoff  = tp->dstY;
		    pr->height = iband->format->height;
		} else {
		    pr->dyoff  = 0;
		    pr->height = iband->format->height - (-tp->dstY);
		}
		if (pr->dyoff + pr->height > dst_height)/* clip bottom */
		    pr->height = dst_height - pr->dyoff;
		    
		/* Adjust thresholds if necessary */
        	if (tp->dstY != pvt->nextline) {
	            if (tp->dstY > pvt->nextline) 
	        	IgnoreBand(iband);
	            else 
	                SetBandThreshold(iband,-tp->dstY+1);
	        }
	    }
	}
  }

  return( TRUE );
}                               /* end InitializePasteUp */

/*------------------------------------------------------------------------
------------------------ crank some single input data --------------------
------------------------------------------------------------------------*/
static int ActivatePasteUp(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  pPasteUpDefPtr pvt   = (pPasteUpDefPtr) ped->elemPvt;
  PasteUpFloat *fconst = pvt->constant;
  receptorPtr rcp      = pet->receptor;
  CARD32 bands         = rcp[SRCt1].inFlo->bands;
  bandPtr dbnd         = &pet->emitter[0];
  mpPasteUpPvtPtr mpvt = (mpPasteUpPvtPtr) pet->private;
  pointer src, dst;
  PasteRectPtr tp;
  CARD32 t, b;
  
    for(b = 0; b < bands; b++, dbnd++, mpvt++, fconst++) {
	INT32 dst_width = dbnd->format->width;

	/* Get pointer for dst scanline, Fill with constant */
	if (!(dst = GetCurrentDst(flo,pet,dbnd)))
	    break;

	(*(mpvt->fill)) (dst,*fconst,mpvt->iconstant,dst_width);

	/* Skip any constant lines */
	if (dbnd->current < mpvt->nextline) {
	    while (dbnd->current < mpvt->nextline)  {
		if (dst = GetNextDst(flo,pet,dbnd,KEEP)) {
		    (*(mpvt->fill)) (dst,*fconst,mpvt->iconstant,dst_width);
		} else {
		    PutData(flo,pet,dbnd,dbnd->current);
		    return TRUE;
		}
	    }
	}

	mpvt->nextline = dbnd->format->height;
  	tp = mpvt->rects;
	for (t = 0; t < mpvt->numRects; t++, tp++){
	    bandPtr sbnd = &rcp[tp->receptorIndex].band[b];
	    INT32 tdy = tp->dyoff;
	    INT32 tdend = tdy + tp->height;

	    if (!tp->active) continue;

	    /* see if this src has a line for the destination */
	    if ((INT32)dbnd->current >= tdy && (INT32)dbnd->current < tdend) {

	        if (sbnd->threshold > 1) {
		    src = GetSrc(flo,pet,sbnd,sbnd->threshold - 1,KEEP);
		    SetBandThreshold(sbnd,1);
		} else 
		    src = GetCurrentSrc(flo,pet,sbnd);
		if (!src) 	/* all tiles for this line should be ready */
		    ImplementationError(flo, ped, return(FALSE));

	        (*(mpvt->action)) (src, tp->sxoff, dst, tp->dxoff, tp->width);

		FreeData(flo,pet,sbnd,sbnd->current+1);
		if ((dbnd->current + 1) < tdend)
		    mpvt->nextline = dbnd->current + 1;
		else
		    tp->active = FALSE;
	    } else if (tdy == (dbnd->current + 1)) {
		/* Tile missed on this line but will be needed for next line */
		AttendBand(sbnd);
		mpvt->nextline = dbnd->current + 1;
	    } else if (tdy < mpvt->nextline) {
		mpvt->nextline = tdy;
	    }
	}
		
	if (mpvt->nextline < dbnd->format->height) {
	    /* ... still more tiles to copy */
	    (void) GetNextDst(flo,pet,dbnd,FLUSH);
	    if (mpvt->nextline != dbnd->current) {
	        /* ... find the Next bunch of tiles */
  		tp = mpvt->rects;
		for (t = 0; t < mpvt->numRects; t++, tp++)
		    if (tp->active && tp->dyoff == mpvt->nextline) {
		        AttendBand(&rcp[tp->receptorIndex].band[b]);
		    }
	    }
	} else {
	    /* ... fill in remaining destination with constant */
	    while((dst = GetNextDst(flo,pet,dbnd,KEEP)))
	        (*(mpvt->fill)) (dst,*fconst,mpvt->iconstant,dst_width);
	    PutData(flo,pet,dbnd,dbnd->current);
	} 

    }
    return(TRUE);
}                               /* end ActivatePasteUp */

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetPasteUp(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  peTexPtr	  pet = ped->peTex;
  mpPasteUpPvtPtr pvt = (mpPasteUpPvtPtr)pet->private;
  CARD32 nbands       = pet->receptor[SRCt1].inFlo->bands;
  CARD32 b;

  /* Free any lists that were malloced */
  for (b = 0; b < nbands; b++, pvt++)
	if (pvt->rects)
		pvt->rects = (PasteRectPtr)XieFree(pvt->rects);

  ResetReceptors(ped);
  ResetEmitter(ped);

  
  return(TRUE);
}                               /* end ResetPasteUp */

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyPasteUp(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* get rid of the peTex structure  */
  ped->peTex = (peTexPtr) XieFree(ped->peTex);
  
  /* zap this element's entry point vector */
  ped->ddVec.create     = (xieIntProc)NULL;
  ped->ddVec.initialize = (xieIntProc)NULL;
  ped->ddVec.activate   = (xieIntProc)NULL;
  ped->ddVec.flush      = (xieIntProc)NULL;
  ped->ddVec.reset      = (xieIntProc)NULL;
  ped->ddVec.destroy    = (xieIntProc)NULL;
  
  return(TRUE);
}                               /* end DestroyPasteUp */

/*------------------------------------------------------------------------
--------------------------PasteUp fill routines  ------------------------
------------------------------------------------------------------------*/
static void FillReal(dst,ffill,ifill,width) 
	RealPixel *dst;
	PasteUpFloat ffill;
	CARD32	ifill,width;
{
	while (width-- > 0) *dst++ = ffill;			
}

#define PasteFill(fname,stype)					\
static void fname(din,ffill,ifill,width)	 		\
pointer din;							\
PasteUpFloat ffill;						\
CARD32 ifill,width;						\
{								\
stype *dst = (stype *)din, fill = (stype)ifill;			\
	while (width-- > 0) *dst++ = fill;			\
}

PasteFill(FillQuad,QuadPixel)
PasteFill(FillPair,PairPixel)
PasteFill(FillByte,BytePixel)


static void FillBit(dst,ffill,ifill,width) 
	BitPixel *dst;
	PasteUpFloat ffill;
	CARD32	ifill,width;
{
	memset((char *)dst, (ifill ? ~0 : 0), (width+7)>>3);
}

/*------------------------------------------------------------------------
--------------------------PasteUp action routines  ----------------------
------------------------------------------------------------------------*/

#define PasteAction(fname,stype)				\
static void fname(sin,s_off,din,d_off,width) 			\
CARD32 width, s_off, d_off;					\
pointer sin, din;						\
{								\
stype *src = (stype *)sin, *dst = (stype *)din;			\
	src += s_off;						\
	dst += d_off;						\
	while (width-- > 0) *dst++ = *src++;			\
}

PasteAction(PasteReal,RealPixel)
PasteAction(PasteQuad,QuadPixel)
PasteAction(PastePair,PairPixel)
PasteAction(PasteByte,BytePixel)

static void PasteBit(sin,s_off,din,d_off,width)
	CARD32 width, s_off, d_off;
	pointer sin, din;
{
	LogInt * src = (LogInt *) sin;
	LogInt * dst = (LogInt *) din;
	for ( ; width-- > 0 ; s_off++, d_off++ ) {
	    if (LOG_tstbit(src,s_off))
		LOG_setbit(dst,d_off);
	    else
		LOG_clrbit(dst,d_off);
	}
}

/* end module mppaste.c */
