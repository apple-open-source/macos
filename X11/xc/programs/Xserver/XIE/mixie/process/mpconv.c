/* $Xorg: mpconv.c,v 1.4 2001/02/09 02:04:30 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module mpconv.c ****/
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
  
	mpconv.c -- DDXIE convolve element
  
	Dean Verheiden && Robert NC Shelley -- AGE Logic, Inc. June, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mpconv.c,v 3.5 2001/12/14 19:58:44 dawes Exp $ */


#define _XIEC_MPCONV
#define _XIEC_PCONV

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
#include <flostr.h>
#include <texstr.h>
#include <technq.h>
#include <memory.h>


/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzeConvolve();

/*
 *  routines used internal to this module
 */
static int CreateConvolveConstant();
static int InitializeConvolveConstant();
static int ActivateConvolveConstant();
static int ResetConvolveConstant();
static int DestroyConvolve();

/*
 * DDXIE Convolve entry points
 */
static ddElemVecRec ConvolveConstantVec = {
  CreateConvolveConstant,
  InitializeConvolveConstant,
  ActivateConvolveConstant,
  (xieIntProc)NULL,
  ResetConvolveConstant,
  DestroyConvolve
  };

typedef struct _mpconvconst {
	pointer carray;		/* stores constant convolution lines */
	ConvFloat *sums;	/* stores constant * kernel vals     */
	void (*action)();	/* band specific action function     */	    
	ConvFloat minClip;	/* constrained data min clip value   */
	ConvFloat maxClip;	/* constrained data max clip value   */
} mpConvConstRec, *mpConvConstPtr;

static void RealConvolveConstant();
static void QuadConvolveConstant();
static void PairConvolveConstant();
static void ByteConvolveConstant();
/*------------------------------------------------------------------------
------------------------  fill in the vector  ---------------------------
------------------------------------------------------------------------*/
int miAnalyzeConvolve(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* based on the technique, fill in the appropriate entry point vector
   */
  switch(ped->techVec->number) {
  case	xieValConvolveConstant:
    ped->ddVec = ConvolveConstantVec;
    break;

  case	xieValConvolveReplicate:
  default:
    return (FALSE);	/* Not implemented in the SI */
  }
  return(TRUE);
}                               /* end miAnalyzeConvolve */


/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateConvolveConstant(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* Allocate space for an array of pointers to constant convolution lines */
  return(MakePETex(flo, 
		   ped, 
		   sizeof(mpConvConstRec) * ped->inFloLst[SRCtag].bands, 
		   SYNC, 
		   NO_SYNC));
}                               /* end CreateConvolve */


#define GenConst(itype)  						    \
	CARD32 iwidth = bnd->format->width;				    \
	itype *cline; 							    \
	register itype cconst = (itype)*tconst;		 	    	    \
	register CARD32 i;					 	    \
	if (!(cline = (itype *)XieMalloc(sizeof(itype) * iwidth)))	    \
		AllocError(flo,ped,return(FALSE));		 	    \
        cpvt->carray = (pointer)cline;					    \
	for (i = 0; i < iwidth; i++) *cline++ = cconst;	 	    	    \
	for (i = 0; i < ks2; i++) bnd->dataMap[i] = (CARD8 *)cpvt->carray;  


/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeConvolveConstant(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
 xieFloConvolve  *raw = (xieFloConvolve *)ped->elemRaw;
 CARD8            msk = raw->bandMask;
 INT32          ksize = raw->kernelSize;
 ConvFloat    *tconst = (ConvFloat *)ped->techPvt;
 ConvFloat    *kernel = (ConvFloat *)ped->elemPvt;
 peTexPtr         pet = ped->peTex;
 mpConvConstPtr  cpvt = (mpConvConstPtr)pet->private;
 receptorPtr      rcp = pet->receptor;
 INT32         b, ks2 = ksize>>1;
 bandPtr bnd;

    /* If processing domain, allow replication */
    if (raw->domainPhototag)
         pet->receptor[ped->inCnt-1].band[0].replicate = msk;
 
    if(!(InitReceptor(flo, ped, &rcp[SRCtag], ksize, ks2+1, msk, ~msk) &&
	 InitProcDomain(flo, ped, raw->domainPhototag, raw->domainOffsetX,
         			                       raw->domainOffsetY) &&
         InitEmitter(flo,ped,SRCtag,NO_INPLACE)))
      return(FALSE);

    /* Create constants for each band */
    for(b = 0; b < ped->inFloLst[SRCtag].bands; b++, cpvt++, tconst++) {
	int i, j;
	ConvFloat *sums;

	if ((msk & (1<<b)) == 0) continue;

        /* Precompute constants for columns falling outside of image */ 
        if (!(cpvt->sums = sums = 
			(ConvFloat *)XieMalloc(sizeof(ConvFloat) * ksize)))
	    AllocError(flo,ped,return(FALSE));

    	for (i = 0; i < ksize; i++) {
	    sums[i] = 0;	
	    if (i == ks2) continue;	/* Don't need a middle */
	    for (j = 0; j < ksize; j++)
		sums[i] += kernel[j * ksize + i] * *tconst;
    	}
	/* Accumulate totals for multiple columns */
	for (i = ks2 - 2; i > -1; i--) 
		sums[i] += sums[i+1];
	for (i = ks2 + 2; i < ksize; i++)
		sums[i] += sums[i-1];

        bnd = &pet->receptor[SRCtag].band[b];
	 switch (bnd->format->class) {
	 case	UNCONSTRAINED:  
		{
			GenConst(RealPixel);
			cpvt->action  = RealConvolveConstant;
			cpvt->minClip = 0; /* Unsued for RealPixels */
			cpvt->maxClip = 0; /* Unused for RealPixels */
		}
		break;
	 case	QUAD_PIXEL:	
		{
			GenConst(QuadPixel);
			cpvt->action  = QuadConvolveConstant;
			cpvt->minClip = 0;
			cpvt->maxClip = (ConvFloat)bnd->format->levels - 1;
		}
	    	break;
	 case	PAIR_PIXEL:	
		{
			GenConst(PairPixel);
			cpvt->action  = PairConvolveConstant;
			cpvt->minClip = 0;
			cpvt->maxClip = (ConvFloat)bnd->format->levels - 1;
		}
		break;
	 case	BYTE_PIXEL:	
		{
			GenConst(BytePixel);
			cpvt->action  = ByteConvolveConstant;
			cpvt->minClip = 0;
			cpvt->maxClip = (ConvFloat)bnd->format->levels - 1;
		}
		break;
	 case	BIT_PIXEL:	/* SILLY_BITONAL, not supported */
	 default:
		return(FALSE);
	 }
    }  
 return(TRUE);
}                               /* end InitializeConvolveConstant */

#define ConvConstAction_Body(dtype)					      \
    /* Handle the left edge */						      \
    endx = (k2 <= currX + run) ? k2 : currX + run;		      	      \
    for(i = currX; i < endx; i++, currX++, run--){		      	      \
	ConvFloat count = 0.0;						      \
        for(j = 0 ; j < ks; j++)					      \
            for(k = -i; k <= k2; k++)					      \
	        count += br[j][i+k] * kernel[j * ks + k + k2]; 		      \
        if (*tconst) 							      \
	    count += cpvt->sums[i]; /* Sum of left side pad */ 		      \
	ClipIt;								      \
        *dst++ = (dtype) count;						      \
    }									      \
    if (run <= 0) return;						      \
    /* Handle the middle */						      \
    endx = (w <=  currX + run) ? w : currX + run;			      \
    for(i = (k2 < currX) ? currX : k2; i < endx; i++,currX++,run--) {	      \
	ConvFloat count = 0.0;						      \
	for(j = 0 ; j < ks; j++)					      \
	    for(k = -k2; k <= k2; k++)					      \
		count += br[j][i+k] * kernel[j * ks + k + k2];		      \
	ClipIt;								      \
        *dst++ = (dtype) count;						      \
    }									      \
    if (run <= 0) return;						      \
    /* Handle the right edge */						      \
    endx = (width <= currX + run) ? width : currX + run;		      \
    for(i = currX; i < endx; i++,currX++,run--) {			      \
        ConvFloat count = 0.0;						      \
	for(j = 0 ; j < ks; j++)					      \
	    for(k = -k2; k < width - i; k++)				      \
		count += br[j][i+k] * kernel[j * ks + k + k2];		      \
	if (*tconst)							      \
            count += cpvt->sums[ks - (width - i)];			      \
	ClipIt;								      \
        *dst++ = (dtype) count;						      \
    }									      \

#define ConvConstAction(name,dtype)				      	      \
static void name(cpvt,kernel,tconst,run,currX,br,dst,ks,width)	      	      \
mpConvConstPtr cpvt;							      \
ConvFloat *kernel, *tconst;						      \
INT32 run, currX, ks;						      	      \
dtype *dst, **br;							      \
CARD32 width;								      \
{									      \
    ConvFloat minClip = cpvt->minClip;					      \
    ConvFloat maxClip = cpvt->maxClip;					      \
    INT32 k2 = ks/2;							      \
    INT32  w = width - k2;      					      \
    INT32 endx;								      \
    int i,j,k;								      \
    /* Adjust destination to current location */			      \
    dst += currX;							      \
    /* Outside of region, pass these pixels */				      \
    if (run < 0) {							      \
        memcpy(dst, &br[k2][currX], -run * sizeof(dtype));		      \
	return;								      \
    }									      \
    ConvConstAction_Body(dtype);					      \
}


/* Macro for clipping of constrained data */
#define ClipIt								      \
if (count < minClip)							      \
    count = minClip;							      \
else if (count > maxClip) 						      \
    count = maxClip;

ConvConstAction(ByteConvolveConstant,BytePixel)
ConvConstAction(PairConvolveConstant,PairPixel)
ConvConstAction(QuadConvolveConstant,QuadPixel)
#undef ClipIt
#define ClipIt
ConvConstAction(RealConvolveConstant,RealPixel)

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateConvolveConstant(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  xieFloConvolve *raw = (xieFloConvolve *)ped->elemRaw;
  mpConvConstPtr cpvt = (mpConvConstPtr)pet->private;
  bandPtr       iband = &pet->receptor[SRCtag].band[0];
  bandPtr       oband = &pet->emitter[0];
  ConvFloat   *tconst = (ConvFloat *)ped->techPvt;
  ConvFloat   *kernel = (ConvFloat *)ped->elemPvt;			
  INT32            ks = raw->kernelSize;
  INT32            k2 = ks/2;
  CARD8         bmask = raw->bandMask;
  int b; 

    for(b = 0; b < ped->inFloLst[SRCtag].bands; b++, cpvt++, tconst++, iband++,
						oband++) 
      if(bmask & 1<<b && pet->scheduled & 1<<b) {

        CARD32    end = iband->format->height - 1 - k2;
	CARD32  width = oband->format->width;
        CARD32  sline = iband->current;
        CARD32  dline = oband->current;
        CARD32    len = ks;
        CARD32    map = 0;
        pointer *br = (pointer *)iband->dataMap;
	pointer dst;
        Bool ok;

	while(!ferrCode(flo)) {
	    INT32 run, currX = 0;

	    /* do special handling at top and bottom of image */
	    if(dline <= k2) {
		len = iband->threshold;
                map = ks - len;
		sline = 0;
		if(dline < k2)
       	            SetBandThreshold(iband, len + 1);
	    }
	   if(dline > end) {
		len = iband->threshold - 1;
		br[len] = (pointer)cpvt->carray;
       	        SetBandThreshold(iband, len);
 	    }

	    ok  = MapData(flo,pet,iband,map,sline++,len,TRUE);
	    dst = GetDst(flo,pet,oband,dline++,TRUE);

	    if(!ok || !dst || !SyncDomain(flo,ped,oband,FLUSH)) break;

	    while (run = GetRun(flo,pet,oband)) {
		(*cpvt->action)(cpvt,kernel,tconst,run,currX,br,dst,ks,width);
		currX += (run > 0) ? run : -run;
	    }
	}
	FreeData(flo, pet, iband, len ? iband->current : iband->maxGlobal);
   }
  return(TRUE);
}                               /* end ActivateConvolveConstant */


/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetConvolveConstant(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  mpConvConstPtr  cpvt = (mpConvConstPtr)ped->peTex->private;
  int b;

  for(b = 0; b < ped->inFloLst[SRCtag].bands; b++, cpvt++) {
  	if (cpvt->carray) cpvt->carray = (pointer) XieFree(cpvt->carray);
	if (cpvt->sums)     cpvt->sums = (ConvFloat *) XieFree(cpvt->sums);
	cpvt->action = (void (*)())NULL;
	cpvt->minClip = 0;
	cpvt->maxClip = 0;
  }

  ResetReceptors(ped);
  ResetEmitter(ped);

  return(TRUE);
}                               /* end ResetConvolveConstant */

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyConvolve(flo,ped)
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
}                               /* end DestroyConvolve */

/* end module mpconv.c */
