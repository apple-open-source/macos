/* $Xorg: mpgeomnn.c,v 1.3 2000/08/17 19:47:53 cpqbld Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module mpgeomnn.c ****/
/******************************************************************************

Copyright 1993, 1994, 1998  The Open Group

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

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
  
	mpgeomnn.c -- DDXIE geometry element for handling nearest
			neighbor technique
  
	Ben Fahy && Larry Hare -- AGE Logic, Inc. June, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mpgeomnn.c,v 3.4 2001/01/17 22:13:12 dawes Exp $ */


#define _XIEC_MPGEOM
#define _XIEC_PGEOM

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
/* #include <mpgeom.h> */
#include <technq.h>
#include <memory.h>

/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzeGeomNN();

/*
 *  routines used internal to this module, technique dependent
 */

static int CreateGeomNN();
static int InitializeGeomNN();
static int ActivateGeomNN();
static int ResetGeomNN();
static int DestroyGeomNN();

/*
 * DDXIE Geometry entry points
 */
static ddElemVecRec NearestNeighborVec = {
  CreateGeomNN,
  InitializeGeomNN,
  ActivateGeomNN,
  (xieIntProc)NULL,
  ResetGeomNN,
  DestroyGeomNN
  };

/*
 * private
 */
#define	PIX0	((double)(0.0000))

typedef struct _mpgeombanddef {

	double	first_mlow,	/* lowest  input line mapped by first output */
		first_mhigh;	/* highest input line mapped by first output */
	int	first_ilow,	/* rounded first_mlow   */
		first_ihigh;	/* rounded first_mhigh  */

	double	*s_locs;	/* useful data precalculated for scaling */
	int	*x_locs;	/* useful data precalculated for scaling */
	int	x_start;
	int	x_end;
	int	int_constant;	/* precalculated for Constrained data fill */
	RealPixel flt_constant;	/* precalculated for UnConstrained data fill */

	int	yOut;		/* what output line we are on */
	int	out_width;	/* output image size */
	int	out_height;	/* ... not used */
	int	in_width;	/* input image size */
	int	in_height;

	int	lo_src_available; /* which input lines we've come across */
	int	hi_src_available;

	void	(*linefunc) ();
	void	(*fillfunc) ();
} mpGeometryBandRec, *mpGeometryBandPtr;

typedef struct _mpgeometrydef {
  	int	upside_down;
  	mpGeometryBandPtr bandInfo[xieValMaxBands];
} mpGeometryDefRec, *mpGeometryDefPtr;

static void FL_R(), FL_b(), FL_B(), FL_P(), FL_Q();
static void (*fill_lines[5])()  = {FL_R, FL_b, FL_B, FL_P, FL_Q,};

static void SL_R(), SL_b(), SL_B(), SL_P(), SL_Q();
static void GL_R(), GL_b(), GL_B(), GL_P(), GL_Q();
static void (*scale_lines[5])() = {SL_R, SL_b, SL_B, SL_P, SL_Q,};
static void (*ggen_lines[5])()  = {GL_R, GL_b, GL_B, GL_P, GL_Q,};

#if XIE_FULL
static void BiSL_R(), BiSL_b(), BiSL_B(), BiSL_P(), BiSL_Q();
static void BiGL_R(), BiGL_b(), BiGL_B(), BiGL_P(), BiGL_Q();
static void (*biscale_lines[5])() = {BiSL_R, BiSL_b, BiSL_B, BiSL_P, BiSL_Q,};
static void (*bigen_lines[5])()   = {BiGL_R, BiGL_b, BiGL_B, BiGL_P, BiGL_Q,};
#endif

/*------------------------------------------------------------------------
------------------------  fill in the vector  ---------------------------
------------------------------------------------------------------------*/
#if XIE_FULL
int miAnalyzeGeomBi(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
   switch(ped->techVec->number) {
   case xieValGeomBilinearInterp:
	ped->ddVec = NearestNeighborVec;	/* Yes */
	break;
   default:
    	return(FALSE);
   }
  return(TRUE);
}                               /* end miAnalyzeGeomBi */
#endif

int miAnalyzeGeomNN(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
   switch(ped->techVec->number) {
   case xieValGeomNearestNeighbor:
	ped->ddVec = NearestNeighborVec;
	break;
   default:
    	return(FALSE);
   }
  return(TRUE);
}                               /* end miAnalyzeGeomNN */


/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateGeomNN(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* allocate space for private data */
  return(MakePETex(flo, ped, sizeof(mpGeometryDefRec), NO_SYNC, NO_SYNC));
}                               /* end CreateGeomNN */

/*------------------------------------------------------------------------
---------------------------- free private data . . . ---------------------
------------------------------------------------------------------------*/
static int FreeBandData(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  mpGeometryDefPtr pvt = (mpGeometryDefPtr) (ped->peTex->private);
  int band, nbands = ped->inFloLst[SRCtag].bands;
  
/*
 *  Look for private data to free
 */
  for (band = 0 ; band < nbands ; band++) { 
    mpGeometryBandPtr pvtband = pvt->bandInfo[band];
    if (pvtband) {
      if (pvtband->x_locs)
	XieFree(pvtband->x_locs);
      if (pvtband->s_locs)
	XieFree(pvtband->s_locs);
      pvt->bandInfo[band] = (mpGeometryBandPtr) XieFree(pvtband);
    }
  }
}

/*------------------------------------------------------------------------
--------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeGeomNN(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  peTexPtr	    pet = ped->peTex;
  mpGeometryDefPtr  pvt = (mpGeometryDefPtr) (pet->private);
  xieFloGeometry    *raw = (xieFloGeometry *) (ped->elemRaw);
  pGeomDefPtr       pedpvt = (pGeomDefPtr) (ped->elemPvt); 
  bandPtr 	    iband = &(pet->receptor[SRCtag].band[0]);
  bandPtr	    oband = &(pet->emitter[0]);
  int		    band, nbands = ped->inFloLst[SRCtag].bands;
  mpGeometryBandPtr pvtband;
#if XIE_FULL
  BOOL		    bilinear = (ped->techVec->number ==
					xieValGeomBilinearInterp);
#endif
 /*
  * x_in = a * x_out + b * y_out + tx 	
  * y_in = c * x_out + d * y_out + ty
  */
  double a  = pedpvt->coeffs[0];
  double b  = pedpvt->coeffs[1];
  double c  = pedpvt->coeffs[2];
  double d  = pedpvt->coeffs[3];
  double tx = pedpvt->coeffs[4];
  double ty = pedpvt->coeffs[5];
  int threshold;
  
/*
 *  Initialize parameters for tracking input lines, etc.
 */
  pvt->upside_down = (d < 0.0);

  for (band = 0 ; band < nbands ; band++, iband++, oband++) { 
    if (pedpvt->do_band[band]) {
	CARD32 dataclass = pet->emitter[band].format->class;

        pvt->bandInfo[band] = pvtband =
		  (mpGeometryBandPtr) XieCalloc(sizeof(mpGeometryBandRec));
	if (!pvtband) {
	   FreeBandData(flo,ped);
  	   AllocError(flo, ped, return(FALSE));
	}
	if (IsConstrained(dataclass))
	    pvtband->int_constant = ConstrainConst(pedpvt->constant[band],
					   pet->emitter[band].format->levels);
	else
	    pvtband->flt_constant = (RealPixel) pedpvt->constant[band];	

	pvtband->fillfunc = 
		fill_lines[IndexClass(dataclass)];
#if XIE_FULL
	pvtband->linefunc = bilinear
		? bigen_lines[IndexClass(dataclass)]
		: ggen_lines[IndexClass(dataclass)];
#else
	pvtband->linefunc = ggen_lines[IndexClass(dataclass)];
#endif
	pvtband->out_width = oband->format->width;
	pvtband->in_width = iband->format->width;
	pvtband->in_height = iband->format->height;

	if (c == 0 && b == 0 ) {
	   int	   in_width = pvtband->in_width;
	   int	   width = pvtband->out_width;
	   int	   x, in_x_coord;
	   double  in_x;

	   /*    For Scaling, can precalculate a lot 	*/

	   if (a == 1 && d == 1) {
	       /* just Cropping, no real resampling to be done */
	   }
#if XIE_FULL
	   pvtband->linefunc = bilinear
		? biscale_lines[IndexClass(dataclass)]
		: scale_lines[IndexClass(dataclass)];
#else
	   pvtband->linefunc = scale_lines[IndexClass(dataclass)];
#endif
	   if (!(pvtband->x_locs = (int *) XieMalloc(width * sizeof(int)))) {
	      FreeBandData(flo,ped);
  	      AllocError(flo, ped, return(FALSE));
	   }
#if XIE_FULL
	   if (bilinear && !(pvtband->s_locs = (double *)
				XieMalloc(width * sizeof(double)))) {
	      FreeBandData(flo,ped);
  	      AllocError(flo, ped, return(FALSE));
	   }
#endif
	   /*  coordinate of line is   x_in = a * x_out + tx 	*/
	   /*  however, we will map pixel centers to centers,   */
	   /*  so we plug in output pixel location x_out+0.5	*/
	   /*  for output_pixel x_out. Happily, finding the     */
	   /*  nearest pixel centered on the computed input	*/
	   /*  location is then found simply by truncating	*/

	   /* initialize to nonsense values */
	   pvtband->x_start = width;
	   pvtband->x_end   = -1;

	   in_x = a*PIX0 + tx;	/* location of center  */

	   for (x=0; x<width; ++x) {
	   	in_x_coord = in_x;	/* closest input pixel */
	        if (in_x_coord >= 0 && in_x_coord < in_width) {
		   /* this pixel is useful */
		   pvtband->x_end = x;
		   if (pvtband->x_start >= width)
			pvtband->x_start = x;
		   pvtband->x_locs[x] = in_x_coord;
#if XIE_FULL
		   if (bilinear)
		       pvtband->s_locs[x] =  in_x - in_x_coord;
#endif
		} else 
		   pvtband->x_locs[x] = -1; /* ignore this */
		in_x += a;		/* next center location */
	   }
	}

 /*
  * we need to compute the initial input line
  * number ranges for the entire output image and the first output line.
  * Just for fun, we will also compute the range for the last output line.
  * How these limits are computed, of course, may depend on techniqu;
  */

 /*
  * For nearest neighbor, we consider lines to have both width and height.
  * Thus, if an image is of area w x h, we picture the image as being:
  *
  *         x=0                         w-1  w
  *   y=0    ________________________________
  *          |   |   |   |   |  ...  |   |   |
  *   y=1    ---------------------------------
  *
  *
  *   y=h-1  ________________________________
  *          |   |   |   |   |  ...  |   |   |
  *   y=h    ---------------------------------
  *
  * The first pixel on the first output line is therefore (.5,.5) and
  * the last  pixel on the  last output line is (w-.5,w-.5), etc.  We 
  * compute the input line ranges by seeing where the four corners map,
  * and selecting the coordinates of the pixel whose *center* maps most
  * closely.
  *
  */

       /*
        * first line of output image
        */
	{
  	double left_map,right_map;
        left_map  = c * PIX0         + d * PIX0 + ty;
        right_map = c * ((pvtband->out_width-1) + PIX0) + d * PIX0 + ty;
        pvtband->first_mlow  = (left_map <= right_map)? left_map : right_map;
        pvtband->first_mhigh = (left_map >= right_map)? left_map : right_map;
	}

       /* 
        *  coordinates with center closest are just truncated doubles
        */
        pvtband->first_ilow  = pvtband->first_mlow;
        pvtband->first_ihigh = pvtband->first_mhigh;


	/* set threshold so we get all needed src lines */
	/* ...if we need line 256, must ask for 257! */
 	threshold = pvtband->first_ihigh + 1;
#if XIE_FULL
	if (bilinear) threshold++;
#endif
	/* make sure we get something */
	if (threshold < 1)
	    threshold = 1;

	/* but don't ask for stuff we can't ever get! */
	if (threshold > pvtband->in_height)
	    threshold = pvtband->in_height;

	if(!InitBand(flo,ped,iband,pvtband->in_height,threshold,NO_INPLACE))
	    return(FALSE);
    } else {
      /* we're suppose to pass this band thru unscathed */
      BypassSrc(flo,pet,iband);
    }
  }
  return(raw->bandMask ? InitEmitter(flo,ped,NO_DATAMAP,NO_INPLACE) : TRUE);
}                               /* end InitializeGeomNN */
/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateGeomNN(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  peTexPtr	pet = ped->peTex;
  pGeomDefPtr	pedpvt = (pGeomDefPtr) (ped->elemPvt); 
  mpGeometryDefPtr pvt = (mpGeometryDefPtr) (ped->peTex->private);
  bandPtr	oband = &(pet->emitter[0]);
  bandPtr	iband = &(pet->receptor[SRCtag].band[0]);
  int		band, nbands = pet->receptor[SRCtag].inFlo->bands;
  register pointer outp;
#if XIE_FULL
  BOOL		bilinear = (ped->techVec->number == xieValGeomBilinearInterp);
#endif

  for(band = 0; band < nbands; band++, iband++, oband++) {

    mpGeometryBandPtr pvtband = pvt->bandInfo[band];
    int width;

    if (!pvtband || ((pet->scheduled & (1 << band)) == 0) )
	continue;

    width = pvtband->out_width;	
    if (pvt->upside_down) {

	/* we're going backwards, which is actually *simpler*, 
	* because we don't get ANY data until we have ALL data.
	* Thus, first time through, just map everything we have.
	*/
	if (!pvtband->yOut)  {
	    if (!MapData(flo,pet,iband,0,0,iband->maxGlobal,KEEP)) { 
		ImplementationError(flo,ped, return(FALSE));
	    }
	    pvtband->lo_src_available = 0;
	    pvtband->hi_src_available = iband->maxGlobal-1;
	}

	outp = GetCurrentDst(flo,pet,oband);
	while (outp) {
	    int lo_in,hi_in;

	    /* get range of src lines for this output line */
	    lo_in = pvtband->first_ilow;
	    hi_in = pvtband->first_ihigh;
			
	    /* rest of output image is off input image */
	    if ( (hi_in < 0) || (lo_in > pvtband->hi_src_available) )
		(*pvtband->fillfunc)(outp,width,pvtband);
	    else
		/* Compute output pixels for this line */
#if XIE_FULL
		(*pvtband->linefunc)(outp,iband->dataMap,width,
			bilinear ? lo_in : hi_in, pedpvt, pvtband);
#else
		(*pvtband->linefunc)(outp,iband->dataMap,width,
					   hi_in, pedpvt, pvtband);
#endif
	    pvtband->first_mlow  += pedpvt->coeffs[3];
	    pvtband->first_mhigh += pedpvt->coeffs[3];
	    pvtband->first_ilow  = (int) pvtband->first_mlow ;
	    pvtband->first_ihigh = (int) pvtband->first_mhigh;
	    pvtband->yOut++;
	    outp = GetNextDst(flo,pet,oband,TRUE);
	}
	if (oband->final)
	    DisableSrc(flo,pet,iband,FLUSH);

    } else {
	/* 
	** nice normal image progression.  This means that I know when
	** I am done with an input line for the current output line,  I 
	** can purge it,  because it won't be needed for subsequent 
	** output lines.
	*/

	while (!ferrCode(flo)) {		
	    int map_lo;		/* lowest  line mapped by output line */
	    int map_hi;		/* highest line mapped by output line */
            int last_src_line = pvtband->in_height - 1;
	    int threshold;

	    /* access current output line */
	    if (!(outp = GetDst(flo,pet,oband,pvtband->yOut,FLUSH))) {
		if (oband->final)
       		    DisableSrc(flo, pet, iband, FLUSH);
		else if (iband->current != 0)
       		    FreeData(flo, pet, iband, iband->current);
		goto breakout;
	    }

	    map_lo = pvtband->first_ilow;
	    if (map_lo < 0)
		map_lo = 0;

	    map_hi = pvtband->first_ihigh;
#if XIE_FULL
	    if (bilinear) map_hi++;
#endif
	    if (map_hi > last_src_line)
		map_hi = last_src_line;

	    if (map_hi < 0 || map_lo > last_src_line)
		(*pvtband->fillfunc)(outp,width,pvtband);
	    else {

		threshold = map_hi - map_lo + 1;
		if(!MapData(flo,pet,iband,map_lo,map_lo,threshold,KEEP))
		    break;

		if (map_lo != iband->current) 
		    ImplementationError(flo,ped, return(FALSE));

		pvtband->lo_src_available = 0;
		pvtband->hi_src_available = iband->maxGlobal-1;

		(*pvtband->linefunc)(outp,iband->dataMap,
					width,map_lo,pedpvt,pvtband);

	    }

	    pvtband->first_mlow  += pedpvt->coeffs[3]; 
	    pvtband->first_mhigh += pedpvt->coeffs[3];
				
	    /* have to be careful about -0.5 rounding to 0, not -1 */
	    if (pvtband->first_ilow < 0) {

    	        pvtband->first_ilow = (pvtband->first_mlow < 0)
						? -1
						: (int)pvtband->first_mlow;	
	     
    	        pvtband->first_ihigh = (pvtband->first_mhigh < 0)
						? -1
						: (int)pvtband->first_mhigh;
	    } else {
		/* if ilow was positive before, needn't check for negative */
		pvtband->first_ilow  = (int)pvtband->first_mlow;	
		pvtband->first_ihigh = (int)pvtband->first_mhigh;
	    }

	    ++pvtband->yOut;						

   	    if (pvtband->first_ilow > last_src_line) {
		/* rest of output image is off the input image */
		/* we will exit after filling output strip */
		while(outp=GetDst(flo,pet,oband,pvtband->yOut,FLUSH)) {
		    (*pvtband->fillfunc)(outp,width,pvtband);
		    pvtband->yOut++;
		}
		if (oband->final)
		    DisableSrc(flo, pet, iband, FLUSH);
		else  
		    goto breakout;
	    }

	    map_hi = pvtband->first_ihigh;
#if XIE_FULL
	    if (bilinear) map_hi++;
#endif
	    if (map_hi > last_src_line)
		map_hi = last_src_line;

	    threshold = map_hi - iband->current + 1;

	    /* make sure we get something */
	    if (threshold <= 1)
		threshold = 1;

	    /* but don't ask for stuff we can't ever get! */
	    if (threshold > pvtband->in_height)
		threshold = pvtband->in_height;

       	    SetBandThreshold(iband, threshold);
	    if (map_hi >= (int) iband->maxGlobal) {
		/* we need to let someone else generate more data */
		break;
	    }
	}  /* end of while no flo err */
	/* want to make sure we GetSrc at least once before Freeing */
	if (iband->current)
	    FreeData(flo, pet, iband, iband->current);
breakout:
	;
    }   /* end of else on normal order */
  }	/* end of band loop */
  return(TRUE);
}                               /* end ActivateGeometry */

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetGeomNN(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  FreeBandData(flo,ped);
  ResetReceptors(ped);
  ResetEmitter(ped);
  
  return(TRUE);
}                               /* end ResetGeomNN */

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyGeomNN(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* get rid of the peTex structure  */
  ped->peTex = (peTexPtr) XieFree(ped->peTex);

  /* zap this element's entry point vector */
  ped->ddVec.create = (xieIntProc)NULL;
  ped->ddVec.initialize = (xieIntProc)NULL;
  ped->ddVec.activate = (xieIntProc)NULL;
  ped->ddVec.flush = (xieIntProc)NULL;
  ped->ddVec.reset = (xieIntProc)NULL;
  ped->ddVec.destroy = (xieIntProc)NULL;

  return(TRUE);
}                               /* end DestroyGeomNN */

/**********************************************************************
****************************   Fill Routines **************************
**********************************************************************/

static void FL_b (OUTP,width,pvtband)
	pointer OUTP;
	int width;
	mpGeometryBandPtr pvtband;
{
	if (pvtband->int_constant)
		action_set(OUTP, width, 0);
	else
		action_clear(OUTP, width, 0);
}

#define DO_FL(funcname, iotype, CONST)					\
static void funcname (OUTP,width,pvtband)				\
pointer	OUTP;								\
register int	width;							\
mpGeometryBandPtr pvtband;						\
{									\
register iotype constant = (iotype) pvtband->CONST;			\
register iotype *outp	= (iotype *) OUTP;				\
									\
	for ( ; width > 0; width--) *outp++ = constant;			\
}

DO_FL	(FL_R, RealPixel, flt_constant)
DO_FL	(FL_B, BytePixel, int_constant)
DO_FL	(FL_P, PairPixel, int_constant)
DO_FL	(FL_Q, QuadPixel, int_constant)

/**********************************************************************
**********************   Neareast Neighbor - Separable ****************
**********************************************************************/

/* (x,y) separable routines (eg, scale, mirror_x, mirror_y)  */

static void SL_b (OUTP,srcimg,width,sline,pedpvt,pvtband)
	pointer OUTP, *srcimg;
	register int width;
	int sline;
	pGeomDefPtr pedpvt; 
	mpGeometryBandPtr pvtband;
{
	register int	xbeg	= pvtband->x_start;
	register int	xend	= pvtband->x_end;
	register int	*x_locs	= pvtband->x_locs;
	register LogInt *src	= (LogInt *) (srcimg[sline]);
	register LogInt *outp	= (LogInt *) OUTP;
	register LogInt outval, M, fill;
	register int	i= 0, w;

	fill = (pvtband->int_constant ? ~(LogInt)0 : 0);

	for (w = xbeg >> LOGSHIFT; w > 0; w--, i+=LOGSIZE)  *outp++ = fill;

	if (xbeg & LOGMASK)  {
	    outval = BitLeft(fill,LOGSIZE-i);
	    for (i = xbeg, M=LOGBIT(i) ; M && i <= xend ; LOGRIGHT(M), i++)
		if (LOG_tstbit(src,x_locs[i]))
		    outval |= M;
	    if (i > xend) {
	        if (fill) outval |= ~BitLeft(fill,LOGSIZE-i);
		i = (i+LOGMASK) & ~LOGMASK;
	    }
	    *outp++ = outval;
	}

	if ( i <= xend) {
	    w = (xend - i + 1) >> LOGSHIFT;
	    for ( ; w > 0; w--, *outp++ = outval)
		for (outval = 0, M=LOGLEFT ; M ; LOGRIGHT(M), i++)
		    if (LOG_tstbit(src,x_locs[i]))
			    outval |= M;

	    for (outval = 0, M=LOGLEFT; i <= xend; LOGRIGHT(M), i++)
	        if (LOG_tstbit(src,x_locs[i]))
		    outval |= M;

	    if (i & LOGMASK) {
	        if (fill) outval |= ~BitLeft(fill,LOGSIZE-i);
		i = (i+LOGMASK) & ~LOGMASK;
		*outp++ = outval;
	    }
	}
	for ( ; i < width; i += LOGSIZE) *outp++ = fill;
}

#define DO_SL(funcname, iotype, CONST)					\
static void funcname (OUTP,srcimg,width,sline,pedpvt,pvtband)		\
register pointer OUTP;							\
register pointer *srcimg;						\
register int width,sline;						\
pGeomDefPtr pedpvt; 							\
mpGeometryBandPtr pvtband;						\
{									\
register int	xbeg	= pvtband->x_start;				\
register int	xend	= pvtband->x_end;				\
register int	*x_locs	= pvtband->x_locs;				\
register iotype constant = (iotype) pvtband->CONST;			\
register iotype *src	= (iotype *) (srcimg[sline]);			\
register iotype *outp	= (iotype *) OUTP;				\
register int	i;							\
	for (i=0; i <  xbeg; ++i) *outp++ = constant;			\
        for (   ; i <= xend; ++i) *outp++ = src[x_locs[i]];		\
        for (   ; i < width; ++i) *outp++ = constant;			\
}

DO_SL	(SL_R, RealPixel, flt_constant)
DO_SL	(SL_B, BytePixel, int_constant)
DO_SL	(SL_P, PairPixel, int_constant)
DO_SL	(SL_Q, QuadPixel, int_constant)

/**********************************************************************
*************************  Bilinear - Seperable  **********************
**********************************************************************/
#if XIE_FULL

/* note - could use BiGL_b since this is a silly anyway */

static void BiSL_b (OUTP,srcimg,width,sline,pedpvt,pvtband)
pointer		  OUTP;
pointer		  *srcimg;
register int	  width;
int     	  sline;
pGeomDefPtr	  pedpvt; 
mpGeometryBandPtr pvtband;
{
register double s, t, st, result;
register int    isrcpix;
register int	*x_locs	= pvtband->x_locs;
register double *s_locs = pvtband->s_locs;
register LogInt constant, val, M, *ptrIn, *ptrJn;
register LogInt *outp	= (LogInt *) OUTP;
register int 	srcwidth  = pvtband->in_width - 1;

	if ( (sline >= pvtband->hi_src_available) ||
	     (sline <  pvtband->lo_src_available) ) {
	    FL_b(outp, width, pvtband);
	    return;
	}
		
	t = pvtband->first_mlow; t -= ((int)t);
	M=LOGLEFT; val = 0;
    	constant = pvtband->int_constant;
	ptrIn = (LogInt *) srcimg[sline];
	ptrJn = (LogInt *) srcimg[sline+1];
	while ( width > 0 ) { 
	    isrcpix = *x_locs++;
	    s = *s_locs++;
	    if ( (isrcpix >= 0) && (isrcpix < srcwidth) ) {
		st = s * t;
		result = 0.;
		if (LOG_tstbit(ptrIn,isrcpix))   result  =
					  ((double)1. - s - t + st);
		if (LOG_tstbit(ptrIn,isrcpix+1)) result += (s - st);
		if (LOG_tstbit(ptrJn,isrcpix))   result += (t - st);
		if (LOG_tstbit(ptrJn,isrcpix+1)) result += st;
		if (result > 0.5) val |= M;
	    } else if (constant) val |= M;
	    width--;
	    LOGRIGHT(M); if (!M) { *outp++ = val; M=LOGLEFT; val = 0; }
	}
	if (M != LOGLEFT) *outp = val;
}

#define BI_SL(funcname, iotype, CONST, ROUND)				\
static void funcname (OUTP,srcimg,width,sline,pedpvt,pvtband)		\
pointer		OUTP;							\
pointer		*srcimg;						\
register int	width;							\
int		sline;							\
pGeomDefPtr	pedpvt; 						\
mpGeometryBandPtr pvtband;						\
{									\
register int	*x_locs	= pvtband->x_locs;				\
register double *s_locs = pvtband->s_locs;				\
register iotype constant = (iotype) pvtband->CONST;			\
register iotype *outp	= (iotype *) OUTP;				\
register iotype *src	= (iotype *) (srcimg[sline]);			\
register iotype *trc;							\
register int i, j, in_width = pvtband->in_width - 1;			\
register double s, t, st;						\
register iotype val;							\
									\
	t = pvtband->first_mlow; t -= ((int)t);				\
	if (sline >= pvtband->hi_src_available)				\
		trc = src; /* or fill line with constant */		\
	else								\
		trc =  (iotype *) (srcimg[sline+1]);			\
        for (i = 0; i < width; i++) {					\
	    j = x_locs[i];						\
	    s = s_locs[i];						\
	    val = constant;						\
	    if (j >= 0 && j < in_width) {				\
		st = s * t;						\
		val = src[j]   * ((float)1. - s - t + st) +		\
		      src[j+1] * (s - st) +				\
		      trc[j]   * (t - st) +				\
		      trc[j+1] * (st) + ROUND;				\
	    }								\
	    *outp++ = val;						\
	}								\
}

BI_SL	(BiSL_R, RealPixel, flt_constant, (float)0.0)
BI_SL	(BiSL_B, BytePixel, int_constant, (float)0.5)
BI_SL	(BiSL_P, PairPixel, int_constant, (float)0.5)
BI_SL	(BiSL_Q, QuadPixel, int_constant, (float)0.5)
#endif

/**********************************************************************
**********************   Neareast Neighbor - General ******************
**********************************************************************/

/* NOTE: for GL routines, would be better to keep running srcpix,
**	and srcline variables outside of scan line routine.
*/

static void GL_b (OUTP,srcimg,width,sline,pedpvt,pvtband)
pointer OUTP;
pointer *srcimg;
register int width;
int sline;
pGeomDefPtr pedpvt; 
mpGeometryBandPtr pvtband;
{
register double a  = pedpvt->coeffs[0];
register double c  = pedpvt->coeffs[2];
register double srcpix  = a * PIX0 +
			  pedpvt->coeffs[1] * (pvtband->yOut + PIX0) +
			  pedpvt->coeffs[4];
register double srcline = c * PIX0 +
			  pedpvt->coeffs[3] * (pvtband->yOut + PIX0) +
			  pedpvt->coeffs[5];
register int 	isrcline,isrcpix;
register LogInt constant, val, M, *ptrIn;
register LogInt *outp	= (LogInt *) OUTP;
register int 	srcwidth  = pvtband->in_width;
register int 	minline  = pvtband->lo_src_available;
register int 	maxline  = pvtband->hi_src_available;


    	constant = pvtband->int_constant ? ~(LogInt) 0 : 0;
	/* could pull out inner if (constant) */
	M=LOGLEFT; val = constant;
	while ( width > 0 ) { 
	    /* in our coordinate system, truncate does a round */
	    isrcline = srcline;
	    isrcpix  = srcpix;
	    /* prepare for next loop */
	    width--;
	    srcline += c;
	    srcpix  += a;
	    /* if (isrcline,isrcpix) not in src image, fill w/val*/
	    if ( (isrcline >= minline) && (isrcline <= maxline) ) { 
		ptrIn = (LogInt *) srcimg[isrcline];
		if ( (isrcpix >= 0) && (isrcpix < srcwidth) && ptrIn )
		    if (LOG_chgbit(ptrIn,isrcpix,constant))
			val ^= M;
	    }
	    LOGRIGHT(M); if (!M) { *outp++ = val; M=LOGLEFT; val = constant; }
	}
	if (M != LOGLEFT) *outp = val;
}


#define DO_GL(funcname, iotype, CONST)					\
static void funcname (OUTP,srcimg,width,sline,pedpvt,pvtband)		\
pointer OUTP;								\
pointer *srcimg;								\
register int width;							\
int sline;								\
pGeomDefPtr pedpvt; 							\
mpGeometryBandPtr pvtband;						\
{									\
register double a  = pedpvt->coeffs[0];					\
register double c  = pedpvt->coeffs[2];					\
register double srcpix  = a * PIX0 +					\
			  pedpvt->coeffs[1] * (pvtband->yOut + PIX0) +	\
			  pedpvt->coeffs[4];				\
register double srcline = c * PIX0 +					\
			  pedpvt->coeffs[3] * (pvtband->yOut + PIX0) +	\
			  pedpvt->coeffs[5];				\
register int 	isrcline,isrcpix;					\
register iotype constant = (iotype) pvtband->CONST;			\
register iotype *outp	= (iotype *) OUTP;				\
register iotype *ptrIn;							\
register iotype val;							\
/* some variables which describe available input data (for clipping) */	\
register int 	srcwidth  = pvtband->in_width;				\
register int 	minline  = pvtband->lo_src_available;			\
register int 	maxline  = pvtband->hi_src_available;			\
									\
	while ( width > 0 ) { 						\
		isrcline = srcline; 					\
		isrcpix  = srcpix; 					\
		width--; 						\
		srcline += c; 						\
		srcpix  += a; 						\
		val = constant; 					\
		if ( (isrcline >= minline) && (isrcline <= maxline) ) { \
		     ptrIn = (iotype *) srcimg[isrcline];  		\
		     if ( (isrcpix >= 0) &&				\
			  (isrcpix < srcwidth) &&			\
			  ptrIn )					\
			val = ptrIn[isrcpix]; 				\
		}							\
		*outp++ = val; 						\
	}								\
}

DO_GL	(GL_R, RealPixel, flt_constant)
DO_GL	(GL_B, BytePixel, int_constant)
DO_GL	(GL_P, PairPixel, int_constant)
DO_GL	(GL_Q, QuadPixel, int_constant)

/**********************************************************************
*************************  Bilinear - General  ************************
**********************************************************************/
#if XIE_FULL

static void BiGL_b (OUTP,srcimg,width,sline,pedpvt,pvtband)
pointer OUTP;
pointer *srcimg;
register int width;
int sline;
pGeomDefPtr pedpvt; 
mpGeometryBandPtr pvtband;
{
register float s, t, st, result;
register double a  = pedpvt->coeffs[0];
register double c  = pedpvt->coeffs[2];
register double srcpix  = a * PIX0 +
			  pedpvt->coeffs[1] * (pvtband->yOut + PIX0) +
			  pedpvt->coeffs[4];
register double srcline = c * PIX0 +
			  pedpvt->coeffs[3] * (pvtband->yOut + PIX0) +
			  pedpvt->coeffs[5];
register int 	isrcline,isrcpix;
register LogInt constant, val, M, *ptrIn, *ptrJn;
register LogInt *outp	= (LogInt *) OUTP;
register int 	srcwidth  = pvtband->in_width - 1;
register int 	minline  = pvtband->lo_src_available;
register int 	maxline  = pvtband->hi_src_available;

    	constant = pvtband->int_constant;
	M=LOGLEFT; val = 0;
	while ( width > 0 ) { 
	    isrcline = srcline;
	    isrcpix  = srcpix;
	    if ( (isrcline >= minline) && (isrcline < maxline) ) { 
		s = srcpix - isrcpix;
		t = srcline - isrcline;
		ptrIn = (LogInt *) srcimg[isrcline];
		ptrJn = (LogInt *) srcimg[isrcline+1];
		if ( (isrcpix >= 0) && (isrcpix < srcwidth) ) {
		    st = s * t;
		    result = 0.;
		    if (LOG_tstbit(ptrIn,isrcpix))   result  =
					      ((float)1. - s - t + st);
		    if (LOG_tstbit(ptrIn,isrcpix+1)) result += (s - st);
		    if (LOG_tstbit(ptrJn,isrcpix))   result += (t - st);
		    if (LOG_tstbit(ptrJn,isrcpix+1)) result += st;
		    if (result > (float) 0.5) val |= M;
		} else if (constant) val |= M;
	    } else if (constant) val |= M;
	    LOGRIGHT(M); if (!M) { *outp++ = val; M=LOGLEFT; val = 0; }
	    width--;
	    srcline += c;
	    srcpix  += a;
	}
	if (M != LOGLEFT) *outp = val;
}

#define BI_GL(funcname, iotype, CONST, ROUND)				\
static void funcname (OUTP,srcimg,width,sline,pedpvt,pvtband)		\
pointer OUTP;								\
pointer *srcimg;							\
register int width;							\
int sline;								\
pGeomDefPtr pedpvt; 							\
mpGeometryBandPtr pvtband;						\
{									\
register float s, t, st;						\
register double a  = pedpvt->coeffs[0];					\
register double c  = pedpvt->coeffs[2];					\
register double srcpix  = a * PIX0 +					\
			  pedpvt->coeffs[1] * (pvtband->yOut + PIX0) +	\
			  pedpvt->coeffs[4];				\
register double srcline = c * PIX0 +					\
			  pedpvt->coeffs[3] * (pvtband->yOut + PIX0) +	\
			  pedpvt->coeffs[5];				\
register int 	isrcline,isrcpix;					\
register iotype constant = (iotype) pvtband->CONST;			\
register iotype *outp	= (iotype *) OUTP;				\
register iotype *ptrIn, *ptrJn;						\
register iotype val;							\
/* some variables which describe available input data (for clipping) */	\
register int 	srcwidth = pvtband->in_width - 1;			\
register int 	minline  = pvtband->lo_src_available;			\
register int 	maxline  = pvtband->hi_src_available;			\
									\
	/* in our coordinate system, truncate does a round */		\
	while ( width > 0 ) { 						\
		isrcline = srcline; 					\
		isrcpix  = srcpix; /* no fpu?, move down in 'if' */	\
		val = constant; 					\
		if ( (isrcline >= minline) && (isrcline < maxline) ) {	\
		    s = srcpix - isrcpix;				\
		    ptrIn = (iotype *) srcimg[isrcline];  		\
		    t = srcline - isrcline;				\
		    ptrJn = (iotype *) srcimg[isrcline+1];  		\
		    st = s * t;						\
		    if ( (isrcpix >= 0) && (isrcpix < srcwidth) )	\
			val =						\
			    ptrIn[isrcpix]   * ((float)1. - s - t + st) + \
			    ptrIn[isrcpix+1] * (s - st) +		\
			    ptrJn[isrcpix]   * (t - st) +		\
			    ptrJn[isrcpix+1] * (st) + ROUND;		\
		}							\
		/* prepare for next loop */				\
		width--; 						\
		srcline += c; 						\
		srcpix  += a; 						\
		*outp++ = val; 						\
	}								\
}

BI_GL	(BiGL_R, RealPixel, flt_constant, (float)0.0)
BI_GL	(BiGL_B, BytePixel, int_constant, (float)0.5)
BI_GL	(BiGL_P, PairPixel, int_constant, (float)0.5)
BI_GL	(BiGL_Q, QuadPixel, int_constant, (float)0.5)
#endif
/**********************************************************************/

/* end module mpgeomnn.c */
