/* $Xorg: mpgeomaa.c,v 1.4 2001/02/09 02:04:31 xorgcvs Exp $ */
/**** module mpgeomaa.c ****/
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
  
	mpgeomaa.c -- DDXIE element for handling antialias
			geometry technique
  
	Ben Fahy && Larry Hare -- AGE Logic, Inc. July, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mpgeomaa.c,v 3.6 2001/12/14 19:58:45 dawes Exp $ */


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
#include <mpgeom.h>
#include <technq.h>
#include <memory.h>


/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzeGeomAA();

/*
 *  routines used internal to this module, technique dependent
 */

/* antialias by lowpass using boxcar filter*/
static int CreateGeomAA();
static int InitializeGeomAA();
static int ActivateGeomAA();
static int ResetGeomAA();
static int DestroyGeomAA();


/*
 * DDXIE Geometry entry points
 */
static ddElemVecRec AntiAliasVec = {
  CreateGeomAA,
  InitializeGeomAA,
  ActivateGeomAA,
  (xieIntProc)NULL,
  ResetGeomAA,
  DestroyGeomAA
  };

static void AASL_R(), AASL_b(), AASL_B(), AASL_P(), AASL_Q(); /* AA scale */
static void AAGL_R(), AAGL_b(), AAGL_B(), AAGL_P(), AAGL_Q(); /* AA general */
static void XXFL_R(), XXFL_b(), XXFL_B(), XXFL_P(), XXFL_Q(); /* fill */
static void (*aa_scl_lines[5])() = { AASL_R, AASL_b, AASL_B, AASL_P, AASL_Q };
static void (*aa_gen_lines[5])() = { AAGL_R, AAGL_b, AAGL_B, AAGL_P, AAGL_Q };
static void (*fill_lines[5])()   = { XXFL_R, XXFL_b, XXFL_B, XXFL_P, XXFL_Q };

#if XIE_FULL
static void GAGL_R(),           GAGL_B(), GAGL_P(), GAGL_Q(); /* GA general */
static void (*ga_scl_lines[5])() = { GAGL_R, AASL_b, GAGL_B, GAGL_P, GAGL_Q };
static void (*ga_gen_lines[5])() = { GAGL_R, AAGL_b, GAGL_B, GAGL_P, GAGL_Q };
#endif

typedef struct _bounding_rect {
	double 	xmin,xmax,ymin,ymax;
} brect;

#define FLG_A_NOT_ZERO 0x01
#define FLG_B_NOT_ZERO 0x02
#define FLG_C_NOT_ZERO 0x04
#define FLG_D_NOT_ZERO 0x08
#define FLG_BACKWARDS  0x10
#define FLG_SKIP_BAND  0x20

typedef struct _mpaabanddef {
	CARD32  flags;		/* see FLG_xxx above */
	int	yOut;		/* what output line we are on */

	int	first_ilow,	/* rounded first_mlow   */
		first_ihigh;	/* rounded first_mhigh  */

	double	first_mlow,	/* lowest  input line mapped by first output */
		first_mhigh;	/* highest input line mapped by first output */

	brect 	left_br;	/* bounding rectangle, left  side */

	int	*ixmin;		/* useful data precalculated for scaling */
	int	*ixmax;

	int	level_clip;	/* in case we need to clip our pixel */
	int	int_constant;	/* precalculated for Constrained data fill */
	RealPixel flt_constant;	/* precalculated for UnConstrained data fill */

	int	in_width;	/* source image size */
	int	in_height;

	int	lo_src_avail;	/* lines we think are available */
	int	hi_src_avail;

	void    (*linefunc) ();	/* action routines based on pixel size */
	void    (*fillfunc) ();
  }
mpAABandRec, *mpAABandPtr;

/*------------------------------------------------------------------------
------------------------  fill in the vector  ---------------------------
------------------------------------------------------------------------*/
int miAnalyzeGeomAA(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    switch(ped->techVec->number) {
    case xieValGeomAntialias:
    case xieValGeomGaussian:	/* safe, but slow and sometimes ugly */
	ped->ddVec = AntiAliasVec;
	break;
    default:
    	return(FALSE);
    }
    return(TRUE);
}                               /* end miAnalyzeGeomAA */


/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateGeomAA(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    int auxsize = sizeof(mpAABandRec) * xieValMaxBands;

    return(MakePETex(flo, ped, auxsize, NO_SYNC, NO_SYNC));

}                               /* end CreateGeomAA */
/*------------------------------------------------------------------------
---------------------------- free private data . . . ---------------------
------------------------------------------------------------------------*/
static int FreeBandData(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    mpAABandPtr pvtband = (mpAABandPtr) (ped->peTex->private); 
    int band, nbands = ped->inFloLst[SRCtag].bands;
  
    for (band = 0 ; band < nbands ; band++, pvtband++) { 
	if (pvtband->flags & FLG_SKIP_BAND)
	    continue;
	if (pvtband->ixmin != NULL)
	    pvtband->ixmin = (int *) XieFree(pvtband->ixmin);

    }
}
/*------------------------------------------------------------------------
--------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeGeomAA(flo,ped)
  floDefPtr flo;
  peDefPtr  ped;
{
  peTexPtr	   pet = ped->peTex;
  xieFloGeometry  *raw = (xieFloGeometry *)ped->elemRaw;
  pGeomDefPtr	pedpvt = (pGeomDefPtr)ped->elemPvt; 
  mpAABandPtr  pvtband = (mpAABandPtr) (pet->private);
  bandPtr	 iband = &(pet->receptor[SRCtag].band[0]);
  bandPtr	 oband = &(pet->emitter[0]);
  int		nbands = ped->inFloLst[SRCtag].bands;
#if XIE_FULL
  BOOL	      gaussian = (ped->techVec->number == xieValGeomGaussian);
#endif
  int		 band;
 /*
  * access coordinates for y_in = c * x_out + d * y_out + ty
  */
  double a  = pedpvt->coeffs[0];
  double b  = pedpvt->coeffs[1];
  double c  = pedpvt->coeffs[2];
  double d  = pedpvt->coeffs[3];
  double tx = pedpvt->coeffs[4];
  double ty = pedpvt->coeffs[5];
  int width = raw->width;
  int threshold;
  double xmin,xmax,ymin,ymax,tot_ymin,tot_ymax;
  CARD32 dataclass;
  CARD32 band_flags;

/*
 *  Initialize parameters for tracking input lines, etc.
 */
  band_flags = 0;

  if (a != 0.0) band_flags |= FLG_A_NOT_ZERO;
  if (b != 0.0) band_flags |= FLG_B_NOT_ZERO;
  if (c != 0.0) band_flags |= FLG_C_NOT_ZERO;
  if (d != 0.0) band_flags |= FLG_D_NOT_ZERO;
  if (d  < 0.0) band_flags |= FLG_BACKWARDS;

  for (band = 0 ; band < nbands; band++, iband++, oband++, pvtband++) { 

    if (pedpvt->do_band[band]) {

	pvtband->flags = band_flags;
	pvtband->yOut  = 0;		/* what output line we are on */

	dataclass =oband->format->class;
	if (IsConstrained(dataclass))
	    pvtband->int_constant = ConstrainConst(pedpvt->constant[band],
						   iband->format->levels);
	else
	    pvtband->flt_constant = (RealPixel) pedpvt->constant[band];	

	pvtband->fillfunc = fill_lines[IndexClass(dataclass)];
#if XIE_FULL
	pvtband->linefunc = gaussian
		? ga_gen_lines[IndexClass(dataclass)]
		: aa_gen_lines[IndexClass(dataclass)];
#else
	pvtband->linefunc = aa_gen_lines[IndexClass(dataclass)];
#endif

	pvtband->level_clip = oband->format->levels;
	pvtband->in_width = iband->format->width;
	pvtband->in_height = iband->format->height;

 /*
  * THE BASIC IDEA - 
  *
  * We consider each output pixel as describing an *area* of width 1
  * and height 1.  Pixel (xo,yo) thus refers to the locus of points:
  *
  * LocusOut(xo,yo) = { (x,y) | xo <= x < xo+1, yo <= y < yo + 1 }
  *
  * When we map this area back to the input image, each corner maps
  * according to:
  *
  *	x_in = a * x_out + b * y_out + tx,
  *	y_in = c * x_out + b * y_out + ty.
  *
  * Now let P(xo,yo) be described as the point at relative position
  * (p,q) from (xo,yo) in LocusOut(xo,yo).  In other words, 
  *
  *	P(xo,yo) = (xo+p, yo+q)
  *
  * Let M(p,q;xo,yo) be the mapping in input space of P(xo,yo). Then
  * the x coordinate of M(p,q;xo+1,yo) is:
  *
  *	XCoord[ M(p,q:xo+1,yo) ] = a * (x_o+1+p) + b*yo + tx
  *				 = a * (x_o+p) + b*yo + tx + a*p
  * so
  *
  *	XCoord[ M(p,q:xo+1,yo) ] = XCoord[ M(p,q;xo,yo) ] + a*p
  *
  * similarly, 
  *
  *	YCoord[ M(p,q:xo,yo+1) ] = YCoord[ M(p,q;xo,yo) ] + c*q
  *
  * also,
  *
  *	XCoord[ M(p+1,q:xo,yo) ] = XCoord[ M(p,q;xo,yo) ] + a*p
  *	YCoord[ M(p+1,q:xo,yo) ] = XCoord[ M(p,q;xo,yo) ] + c*p
  *	XCoord[ M(p,q+1:xo,yo) ] = YCoord[ M(p,q;xo,yo) ] + b*q
  *	YCoord[ M(p,q+1:xo,yo) ] = YCoord[ M(p,q;xo,yo) ] + d*q
  *
  * We will use these results to derive a computationally simple
  * antialiasing algorithm.
  *
  * Suppose we are scaling an image down by a factor of four in
  * the X direction:
  *
  *  |  |  |  |**|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
  *  |  |  |  |**|  |  |  |  |**|**|**|**|**|**|**|**|**|**|  |
  *  |  |  |  |**|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
  *
  *       |           |          | ******** | ******** | *****
  *
  *  If we nearest neighbor sample the input image where the 
  *  output pixels map to input locations,  the horizontal line
  *  at the right will show up in the output image, but the vertical
  *  line at left will be completely missed.
  *
  *  The idea of this algorithm is to take the average of all pixels
  *  which are included in the *region* mapped by the output *area*,
  *  as opposed to looking at discrete pixels only.  So we consider
  *  a pixel as representing an output locus
  *
  * 	LocusOut(xo,yo) = { (x,y) | xo <= x < xo+1, yo <= y < yo + 1 }
  *
  *  and the input locus will have corner points:
  *
  *	( XCoord[ M(0,0:xo,yo) ],YCoord[ M(0,0:xo,yo) ] )
  *	( XCoord[ M(1,0:xo,yo) ],YCoord[ M(1,0:xo,yo) ] )
  *	( XCoord[ M(0,1:xo,yo) ],YCoord[ M(0,1:xo,yo) ] )
  *	( XCoord[ M(1,1:xo,yo) ],YCoord[ M(1,1:xo,yo) ] )
  *
  *  If  xi(xo) = XCoord[ M(0,0;xo,yo) ]
  *  and yi(yo) = YCoord[ M(0,0;xo,yo) ],
  *
  *  Then we can rewrite these using the relations above as:
  *
  *  	( xi(xo),     yi(yo) ),  
  *	( xi(xo)+a,   yi(yo)+c ), 
  *  	( xi(xo)+b,   yi(yo)+d),  
  *	( xi(xo)+a+b, yi(yo)+c+d ). 
  * 
  *  We have very little guarantee on what shape this collection
  *  of points will assume,  without agreeing to look for special
  *  cases of (a,b,c,d).  However, we do know that whatever the
  *  locus looks like, there is a "bounding rectangle":
  *
  *	BRect: { (xmin,ymin; xmax,ymax) } 
  *
  *  such that xmin is the greatest lower bound of all x in the input locus,
  *  such that ymin is the greatest lower bound of all y in the input locus,
  *  such that xmax is the least upper bound of all x in the input locus, &
  *  such that ymax is the least upper bound of all y in the input locus.
  *
  *  Furthermore, this bounding rectangle has the very nice property 
  *  of "shift invariance," ie, 
  *
  *  If   { (xmin,ymin; xmax,ymax) } 	is the bounding rectangle for (xo,yo),
  *
  *  Then { (xmin+a*m+b*n,ymin+c*m+d*n; xmax+a*m+b*n,ymax+c*m+d*n) } 
  *					is the bounding rect for (xo+m,yo+n).
  *
  *  Proof.  Let Yo be the ymax coordinate of the bounding rectangle for 
  *  (xo,yo). Let ILo be the input locus of (xo,yo).  Let Yo' be the ymax 
  *  coordinate of (xo+m,yo+n),  and ILo' be the input locus of (xo+m,yo+n).  
  *  We claim Yo' = Yo + c*m+d*n. 
  *
  *  By definition, Yo >= y for all y in ILo, and Yo' >= y for all y in ILo'.  
  *  Suppose Yo' < Yo + c*m+d*n.  Then Yo~ = (Yo' - c*m - d*n) is less than
  *  Yo.  If Yo~ >= y for all y in ILo,  then this contradicts Yo being a
  *  least upper bound.  Therefore there must be some offset (r,s) such 
  *  that Yrs = YCoord[ M(r,s;xo,yo) ] is > Yo~.  But by the translation
  *  rules above, YCoord[ M(r,s; xo+m,yo+n) ] = Yrs + c*m + d*n. Call 
  *  this point Yrs'.  Then:
  *
  *     Yrs' = Yrs + c*m + d*n > Yo~ + c*m + d*n = Yo'
  *
  *  which violates the assumption that Yo' is the least upper bound for
  *  ILo'.   Thus, Yo' >= Yo + c*m + d*n.  It is easy to show that Yo'
  *  greater than Yo + c*m + d*n also leads to a contradiction.  Therefore
  *  we have Yo' identically equal to Yo + c*m + d*n,  and the other 
  *  coordinates (xmin,xmax,ymin) follow by similar reasoning.
  *
  * ----------------------------------------------------------------------
  *
  *  WHY THIS IS USEFUL:
  *
  *  We compute the bounding rectangle for the first pixel on the first
  *  output line. Then it is easy to compute the bounding rectangle for
  *  all other points in the output image.  In particular, we can compute
  *  ymin,ymax values for any line in the output image. We don't bother
  *  decoding any output line until all of the required input image lines
  *  are available.
  *
  *  Once we have the data,  we call a line function that does the real
  *  work.  The line function simply marches through the bounding rect
  *  of each pixel in the output line,  adds up the input image numbers,
  *  and divides by the number of discrete pixels in the bounding rect.
  *  The next bounding rect in the line is easily calculated by the
  *  shift-invariance relation.  Clipping of the bounding rect is used
  *  to avoid integrating nonsense values.  An output pixel is filled
  *  with the constant value only if the *entire* bounding rectangle
  *  is off-image.
  *
  */

/***  Calculate bounding rectangle of first pixel, ylimits of first line ***/

	xmin = xmax = tx;
	ymin = ymax = tot_ymin = tot_ymax = ty;

#if XIE_FULL
	if (gaussian) {
	    pTecGeomGaussianDefPtr tkpvt = (pTecGeomGaussianDefPtr)ped->techPvt;

	    if (tkpvt->radius < 1) tkpvt->radius = 2;

	    /* conservatively pick a bounding box for datamgr calculations */
	    xmin -= ((double) tkpvt->radius + 1.00001);
	    xmax += ((double) tkpvt->radius + 1.99999);
	    ymin -= ((double) tkpvt->radius + 1.00001);
	    ymax += ((double) tkpvt->radius + 1.99999);
	    tot_ymin = ymin + (c < 0.0 ? (width * c) : 0.0);
	    tot_ymax = ymax + (c > 0.0 ? (width * c) : 0.0);
	} else
#endif
	{
	    if (a < 0) xmin += a;
	    else       xmax += a;
	    if (b < 0) xmin += b;
	    else       xmax += b;

	    if (c < 0) { ymin += c; tot_ymin += (width * c); }
	    else       { ymax += c; tot_ymax += (width * c); }
	    if (d < 0) { ymin += d; tot_ymin += d; }
	    else       { ymax += d; tot_ymax += d; }
	}

	pvtband->left_br.xmin = xmin;
	pvtband->left_br.ymin = ymin;
	pvtband->left_br.xmax = xmax;
	pvtband->left_br.ymax = ymax;

        pvtband->first_mlow  = tot_ymin;
        pvtband->first_mhigh = tot_ymax;

        pvtband->first_ilow  = pvtband->first_mlow;
        pvtband->first_ihigh = pvtband->first_mhigh;

/***	Check for some special cases 	***/
	if ((band_flags & (FLG_C_NOT_ZERO | FLG_B_NOT_ZERO)) == 0) {
	    int i, maxpixl  = pvtband->in_width - 1;

#if XIE_FULL
	    pvtband->linefunc = gaussian
		? ga_scl_lines[IndexClass(dataclass)]
		: aa_scl_lines[IndexClass(dataclass)];
#else
	    pvtband->linefunc = aa_scl_lines[IndexClass(dataclass)];
#endif

	    if (!(pvtband->ixmin = (int *) XieMalloc(width * 2*sizeof(int)))) {
		FreeBandData(flo,ped);
		AllocError(flo, ped, return(FALSE));
	    }
	    pvtband->ixmax = pvtband->ixmin + width;
	    /* Valid X coord domain will remain same for entire image */
	    for  ( i=0; i<width; ++i) { 
		register int ixmin,ixmax;

		ixmin = xmin; 		
		ixmax = xmax; 
		xmin += a; 
		xmax += a; 

		if (ixmin < 0)		ixmin = 0;
		if (ixmax > maxpixl)	ixmax = maxpixl;
		if (ixmax > ixmin)	ixmax--;

		pvtband->ixmin[i] = ixmin;
		pvtband->ixmax[i] = ixmax;
	    }
	}

	/* set threshold so we get all needed src lines */
	/* if we need line 256, must ask for 257! */
	threshold = pvtband->first_ihigh + 1;

	/* make sure we get something */
	if (threshold < 1)
	    threshold = 1;

	/* but don't ask for stuff we can't ever get! */
	if (threshold > iband->format->height)
	    threshold = iband->format->height;

	if(!InitBand(flo, ped, iband, iband->format->height,
						     threshold, NO_INPLACE))
	    return(FALSE);
    } else {
	pvtband->flags = FLG_SKIP_BAND;
	BypassSrc(flo,pet,iband);
    }
  }
  return(raw->bandMask ? InitEmitter(flo,ped,NO_DATAMAP,NO_INPLACE) : TRUE);
}                               /* end InitializeGeomAA */
/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateGeomAA(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  peTexPtr	  pet = ped->peTex;
  pGeomDefPtr  pedpvt = (pGeomDefPtr) ped->elemPvt; 
  mpAABandPtr pvtband = (mpAABandPtr) pet->private;
  bandPtr	oband = &(pet->emitter[0]);
  bandPtr	iband = &(pet->receptor[SRCtag].band[0]);
  int    band, nbands = pet->receptor[SRCtag].inFlo->bands;
  int    	width = oband->format->width; /* consider use of pvtband */
  double	    d = pedpvt->coeffs[3];
  register pointer outp;

  for(band = 0; band < nbands; band++, iband++, oband++, pvtband++) {

    if (pvtband->flags & FLG_SKIP_BAND)
	continue;       

    if ((pet->scheduled & (1 << band)) == 0)
	continue;       

    if (pvtband->flags & FLG_BACKWARDS) {

	/* we're going backwards, which is actually *simpler*, 
	* because we don't get ANY data until we have ALL data.
	* Thus, first time through, just map everything we have.
	*/
	if (!pvtband->yOut)  {
	    if (!MapData(flo,pet,iband,0,0,iband->maxGlobal,KEEP)) 
		ImplementationError(flo,ped, return(FALSE));
	    pvtband->lo_src_avail = 0;
	    pvtband->hi_src_avail = iband->maxGlobal-1;
	}

	outp = GetCurrentDst(flo,pet,oband);
	while (outp) {

	    if ((pvtband->first_ihigh < 0) ||
		(pvtband->first_ilow  > pvtband->in_height))

		(*pvtband->fillfunc)(outp,width,pvtband);
	    else 
		(*pvtband->linefunc)(outp,iband->dataMap,width,ped,pvtband);

	    /* now compute highest input line for next oline */
	    pvtband->first_mlow  += d;
	    pvtband->first_mhigh += d;
	    pvtband->first_ilow  = (int) pvtband->first_mlow ;
	    pvtband->first_ihigh = (int) pvtband->first_mhigh;
	    pvtband->yOut++;
	    outp = GetNextDst(flo,pet,oband,TRUE);
	}
	if (oband->final)
	    DisableSrc(flo,pet,iband,FLUSH);

    } else {

	while (!ferrCode(flo)) {		
	    int map_lo;		/* lowest  line mapped by output line */
	    int map_hi;		/* highest line mapped by output line */
	    int last_src_line = pvtband->in_height - 1;
	    int threshold;

	    /* access current output line */
	    outp = GetDst(flo,pet,oband,pvtband->yOut,FLUSH);
	    if (!outp) {
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
	    if (map_hi > last_src_line)
		map_hi = last_src_line;

	    if (map_hi < 0 || map_lo > last_src_line)
		(*pvtband->fillfunc)(outp,width,pvtband);
	    else {
	        if(!MapData(flo,pet,iband,map_lo,map_lo,(map_hi-map_lo+1),KEEP))
		    break;
		pvtband->lo_src_avail = 0;
		pvtband->hi_src_avail = iband->maxGlobal-1;

		if (map_lo != iband->current) 
		    ImplementationError(flo,ped, return(FALSE));

		/***	Compute output pixels for this line ***/
		(*pvtband->linefunc)(outp,iband->dataMap,width,ped,pvtband);
	    }

	    /* increment to next line. */

	    pvtband->first_mlow  += d;
	    pvtband->first_mhigh += d;
					
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
		if (oband->final) /* out of destination lines */
		    DisableSrc(flo, pet, iband, FLUSH);
		else  
		    goto breakout;
		/* Be nice and let downstream element eat our data */
		/* notice we don't free input data, because then the */
		/* silly scheduler would turn us off */
	    }

	    map_hi = pvtband->first_ihigh;
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
	}
	/* want to make sure we GetSrc at least once before Freeing */
	if (iband->current)
	    FreeData(flo, pet, iband, iband->current);
    }	/* end of backwards/forwards */
breakout: ;
  }	/* end of band loop */
  return(TRUE);
}                               /* end ActivateGeometry */

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetGeomAA(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    FreeBandData(flo,ped);
    ResetReceptors(ped);
    ResetEmitter(ped);
  
    return(TRUE);
}                               /* end ResetGeomAA */

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyGeomAA(flo,ped)
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

    return(TRUE);
}                               /* end DestroyGeomAA */

/**********************************************************************/
/* fill routines */
static void XXFL_b (OUTP,width,pvtband)
	register pointer OUTP;
	register int width;
	mpAABandPtr pvtband;
{
	register LogInt constant = (LogInt) pvtband->int_constant;
	register LogInt *outp	= (LogInt *) OUTP;
	register int	i;

	if (constant) constant = ~0;
	/*
	** NOTE: Following code assume filling entire line. Which is
	** currently true.  In the future we may need to abide by
	** bit boundaries. Conversely code for bytes and pairs below
	** could be sped up by doing something similar. perhaps we need
	** an action_constant(outp,run,ix,constant) routine similar to
	** action_set().
	*/
	width = (width + 31) >> 5;
	for (i=0; i < width; ++i) *outp++ = constant;
}

#define DO_FL(funcname, iotype, CONST)					\
static void funcname (OUTP,width,pvtband)				\
    register pointer OUTP;						\
    register int width;							\
    mpAABandPtr pvtband;						\
{									\
    register iotype constant = (iotype) pvtband->CONST;			\
    register iotype *outp = (iotype *) OUTP;				\
									\
    for ( ; width > 0; width--) *outp++ = constant;			\
}

DO_FL	(XXFL_R, RealPixel, flt_constant)
DO_FL	(XXFL_B, BytePixel, int_constant)
DO_FL	(XXFL_P, PairPixel, int_constant)
DO_FL	(XXFL_Q, QuadPixel, int_constant)

/**********************************************************************/
/* (x,y) separable routines (eg, scale, mirror_x, mirror_y) 	      */
/**********************************************************************/
/*
 * NOTE - see caveat for GL_b for why the user shouldn't really be
 * asking for antialias in the first place...
 */
static void AASL_b (OUTP,srcimg,width,ped,pvtband)
    register pointer OUTP, *srcimg;
    register int width;
    peDefPtr  ped;
    mpAABandPtr pvtband;
{
    pGeomDefPtr	pedpvt = (pGeomDefPtr)ped->elemPvt; 
    double d = pedpvt->coeffs[3];

    /* These variables describe the limits of the bounding rectangle */

    /* since x and y separable, x limits are precalculated once */
    register int *ixminptr=pvtband->ixmin;
    register int *ixmaxptr=pvtband->ixmax;
    register int ixmin,ixmax;	

    /* since x and y separable, y limits will be set once this line */
    register int iymin,iymax;

    /* some variables which describe available input data (for clipping) */
    register int minline = pvtband->lo_src_avail;
    register int maxline = pvtband->hi_src_avail;


    /* cast of constant, output and input pointers to correct type */
    register LogInt *outp = (LogInt *) OUTP;
    register LogInt *ptrIn;
    register LogInt constant;
    register LogInt value,outval,M;
    register int nfound;

    	constant = pvtband->int_constant ? ~(LogInt) 0 : 0;

	/* round bounding rectangle limits to ints */
	iymin=pvtband->left_br.ymin;
	iymax=pvtband->left_br.ymax;

	/* clip to available src data */
	if (iymin < minline) 	iymin = 0;
	if (iymax > maxline) 	iymax = maxline;
	if (iymax > iymin)	iymax--;

	/* M is a number that encodes the "active" bit */ 
	M=LOGLEFT; 
	
	/* val accumulates the output bits */
#define kludge_based
#ifdef kludge_based
	outval = 0;
#else
	outval = constant;
#endif

	/* loop through all the output pixels on this line */
	while ( width-- > 0 ) { 
	    register int 	ix,iy;

   	    /* make use of precalculated X limits */
	    ixmin = *ixminptr++;
	    ixmax = *ixmaxptr++;

	    nfound = 0;   
	    value = 0;   
	    for (iy=iymin; iy<=iymax; ++iy) {
		ptrIn = (LogInt *) srcimg[iy]; 

		for (ix=ixmin; ix<=ixmax; ++ix) {
		  ++nfound;
#ifdef kludge_based
		  if (LOG_tstbit(ptrIn,ix)) {
		    ++value;
#else
		  if (!LOG_tstbit(ptrIn,ix)) {
		    /* set this pixel black */
		    outval &= ~M;
		    goto next;
#endif
		  }
		}
	    } /* end of iy loop */

/* See GL_b() for an explanation of the kludge below */

#ifdef kludge_based
#define kludge 8/7
    /* note: 4/3 relies on C evaluating left to right */

	    if (!nfound)
		outval |= (constant&M);	/* set to background */
	    else if (value*kludge >= nfound)
		outval |= M;
#else

	    if (!nfound)
		outval &= (constant&M);	/* set to background */
	    else
	        outval |= M;		/* set bit on */
next:

#endif
	    /* shift our active bit over one */
	    LOGRIGHT(M); 
		
	    /* if we hit the end of a word, output what we have */
	    if (!M) { 
		*outp++ = outval; 	/* record output data   */
		M=LOGLEFT; 		/* reset active bit  	*/
		outval = 0; 		/* reset accumulator 	*/
	    }

	} /* end of line loop */

	/* if we didn't write all the pixels out, do so now */
	if (M != LOGLEFT) *outp = outval;

	/* before leaving, update bounding rect for next line */
	pvtband->left_br.ymin += d;
	pvtband->left_br.ymax += d;
}

/* NOTE: too bad paper is white is 255, otherwise you can often avoid the
** divide by "if (value) value /= nfound;"
*/
#define aalinsep_func(funcname, iotype, valtype, CONST)			\
									\
	/* round bounding rectangle limits to ints */			\
	iymin = pvtband->left_br.ymin;					\
	iymax = pvtband->left_br.ymax;					\
									\
	/* clip to available src data */				\
	if (iymin < minline) 	iymin = 0;				\
	if (iymax > maxline) 	iymax = maxline;			\
	if (iymax > iymin)	iymax--;				\
									\
	/* loop through all the output pixels on this line */		\
	for ( i=0; i<width; ++i) { 					\
	    /* loop variables */					\
	    register int ix,iy;						\
									\
	    nfound = 0;     						\
	    value = 0;     						\
	    /* ixmin = pvtband->ixmin[i]; */				\
	    /* ixmax = pvtband->ixmax[i]; */				\
	    ixmin = *ixminptr++;					\
	    ixmax = *ixmaxptr++;					\
									\
	    for (iy=iymin; iy<=iymax; ++iy) {				\
		ptrIn = ixmin + (iotype *) srcimg[iy];  		\
		for (ix=ixmin; ix<=ixmax; ++ix) {			\
		  value += *ptrIn++;					\
		  ++nfound;			  			\
		}				  			\
	    } /* end of iy loop */					\
									\
	    if (nfound) 						\
		value /= nfound;					\
	    else							\
	        value = constant;					\
									\
	    *outp++ = value;						\
									\
	}   /* end of for loop */					\
									\
	/* before leaving, update bounding rect for next line */	\
	pvtband->left_br.ymin += d;					\
	pvtband->left_br.ymax += d;					\

#define DO_AASL(funcname, iotype, valtype, CONST)			\
static void								\
funcname (OUTP,srcimg,width,ped,pvtband)				\
    register pointer OUTP;						\
    register pointer *srcimg;						\
    register int width;							\
    peDefPtr  ped;							\
    mpAABandPtr pvtband;						\
{									\
    pGeomDefPtr	pedpvt = (pGeomDefPtr)ped->elemPvt; 			\
									\
    /* Mapping coefficients */						\
    double d = pedpvt->coeffs[3];					\
									\
    /* These variables describe the limits of the bounding rectangle */	\
    register int *ixminptr = pvtband->ixmin;				\
    register int *ixmaxptr = pvtband->ixmax;				\
    register int ixmin,iymin,ixmax,iymax;				\
									\
    /* variables which describe available input data (for clipping) */	\
    register int minline = pvtband->lo_src_avail;			\
    register int maxline = pvtband->hi_src_avail;			\
									\
    /* cast of constant, output and input pointers to correct type */	\
    register iotype constant = (iotype) pvtband->CONST;			\
    register iotype *outp = (iotype *) OUTP;				\
    register iotype *ptrIn;						\
									\
    register valtype value; 						\
    register int i,nfound;						\
									\
	aalinsep_func(funcname, iotype, valtype, CONST)			\
}

DO_AASL	(AASL_R, RealPixel, RealPixel, flt_constant)
DO_AASL	(AASL_B, BytePixel, QuadPixel, int_constant)
DO_AASL	(AASL_P, PairPixel, QuadPixel, int_constant)
DO_AASL	(AASL_Q, QuadPixel, QuadPixel, int_constant)


/**********************************************************************/
/* general routines (should be able to handle any valid map) */

/* CAVEAT - antialias doesn't really make much sense for bit-bit,
 * because you can't represent any intermediate values.  This means
 * you can choose between line dropouts (bad) or making your image
 * overly bold (also bad).  The algorithm here assumes that if you
 * were worried about being too bold, you would use the default
 * sampling technique (eg, nearest neighbor). So it attempts to
 * eliminate line dropouts at the expense of decreasing fine 
 * image detail.
 */

static void
AAGL_b (OUTP,srcimg,width,ped,pvtband)
    register pointer OUTP, *srcimg;
    register int width;
    peDefPtr  ped;
    mpAABandPtr pvtband;
{
    pGeomDefPtr	pedpvt = (pGeomDefPtr)ped->elemPvt;

    /* Mapping coefficients */
    double a = pedpvt->coeffs[0]; 
    double b = pedpvt->coeffs[1];
    double c = pedpvt->coeffs[2];
    double d = pedpvt->coeffs[3];

    /* These variables describe the limits of the bounding rectangle */
    double xmin = pvtband->left_br.xmin;
    double ymin = pvtband->left_br.ymin;
    double xmax = pvtband->left_br.xmax;
    double ymax = pvtband->left_br.ymax;
    register int ixmin,iymin,ixmax,iymax;	

    /* loop variables for roaming through the bounding rectangle */
    register int ix,iy;

    /* some variables which describe available input data (for clipping) */
    register int maxpixl = pvtband->in_width - 1;
    register int minline = pvtband->lo_src_avail;
    register int maxline = pvtband->hi_src_avail;
    CARD32	 flags	 = pvtband->flags;

    /* cast of constant, output and input pointers to correct type */
    register LogInt *outp = (LogInt *) OUTP;
    register LogInt *ptrIn;
    register LogInt constant;
    register LogInt value,outval,M;
    register int nfound;

    	constant = pvtband->int_constant ? ~(LogInt) 0 : 0;

	/* round bounding rectangle limits to ints */
	ixmin = xmin;
	iymin = ymin;
	ixmax = xmax;
	iymax = ymax; 

	/* clip to available src data */
	if (ixmin < 0) 	 	ixmin = 0;
	if (iymin < minline) 	iymin = 0;
	if (ixmax > maxpixl) 	ixmax = maxpixl;
	if (iymax > maxline) 	iymax = maxline;

	/* M is a number that encodes the "active" bit */ 
	M=LOGLEFT; 
	
	/* val accumulates the output bits */
#define kludge_based_not
#ifdef kludge_based
	outval = 0;
#else
	outval = constant;
#endif

	/* loop through all the output pixels on this line */
	while ( width > 0 ) { 

	    xmin += a; 
	    xmax += a; 
	    nfound = 0;   
	    value = 0;   
	    for (iy=iymin; iy<=iymax; ++iy) {
		ptrIn = (LogInt *) srcimg[iy]; 

		for (ix=ixmin; ix<=ixmax; ++ix) {
		  ++nfound;
#ifdef kludge_based
		  if (LOG_tstbit(ptrIn,ix)) {
		    ++value;
		  }
#else
		  if (!LOG_tstbit(ptrIn,ix)) {
		    /* set this pixel black */
		    outval &= ~M;
		    goto next;
		  }
#endif
		}
	    } /* end of iy loop */

/* on the kludge below:  antialias is weird when you are going from
   1 bit to 1 bit.  A straight generalization of the n-bit algorithm
   would have us find the average value in the bounding rect, and
   then round to 0 or 1.  This corresponds to kludge = 2.  However,
   it may be desirable to choose black if a much smaller number of
   pixels in the bounding rect are off.  For example, we may want to
   have black output if less than 3 out of every four pixels in the
   input bounding rect are on.  (value <= 3/4 nfound) which would
   correspond to value *4/3 < nfound.

   The problem with choosing anything other than 2 is that we are
   treating black and white asymmetrically:   the algorithm will
   perform well for black lines on a white background, and poorly
   for white pixels on a black background.  Granted, one could use
   Point or Constrain to flip, but.... ugh.

   Note that if the bounding rectangle is completely off the input
   image, then we should set 'val' to the background value (constant).
*/

#ifdef kludge_based
#define kludge 8/7
    /* note: 4/3 relies on C evaluating left to right */

	    if (!nfound)
		outval |= (constant&M);	/* set to background */
	    else if (value*kludge >= nfound)
		outval |= M;
#else

	    if (!nfound)
		outval &= (constant&M);	/* set to background */
	    else
	        outval |= M;		/* set bit on */
next:

#endif
	    /* shift our active bit over one */
	    LOGRIGHT(M); 
		
	    /* if we hit the end of a word, output what we have */
	    if (!M) { 
		*outp++ = outval; 	/* record output data   */
		M=LOGLEFT; 		/* reset active bit  	*/
		outval = 0; 		/* reset accumulator 	*/
	    }

	    /* prepare geometry stuff for next loop */
	    width--; 
	    ixmin = xmin;
	    ixmax = xmax;     
	    if (flags & FLG_C_NOT_ZERO) {
		ymin += c; 
		ymax += c; 
		iymin = ymin;     
		iymax = ymax;    
		if (iymin < minline)	iymin = minline;
	       	if (iymax >= maxline)	iymax = maxline;
		if (iymax > iymin)	iymax--;
	    } 
	    if (ixmin < 0)		ixmin = 0;
	    if (ixmax >= maxpixl)	ixmax = maxpixl;
	    if (ixmax > ixmin)	ixmax--;
	} /* end of line loop */

	/* if we didn't write all the pixels out, do so now */
	if (M != LOGLEFT) *outp = outval;

	/* before leaving, update bounding rect for next line */
	if (flags & FLG_B_NOT_ZERO) {
	   pvtband->left_br.xmin += b;
	   pvtband->left_br.xmax += b;
	}
	if (flags & FLG_D_NOT_ZERO) {
	   pvtband->left_br.ymin += d;
	   pvtband->left_br.ymax += d;
	}
}

/* NOTE: too bad paper is white is 255, otherwise you can often avoid the
** divide by "if (value) value /= nfound;"
*/
#define inscrutable_compiler(funcname, iotype, valtype, CONST)		\
	/* round bounding rectangle limits to ints */			\
	ixmin = xmin;		     					\
	iymin = ymin;     						\
	ixmax = xmax;     						\
	iymax = ymax;     						\
									\
	/* clip to available src data */				\
	if (ixmin < 0) 	 	ixmin = 0;				\
	if (iymin < minline) 	iymin = 0;				\
	if (ixmax > maxpixl) 	ixmax = maxpixl;			\
	if (iymax > maxline) 	iymax = maxline;			\
									\
	/* loop through all the output pixels on this line */		\
	while ( width > 0 ) { 						\
									\
	    xmin += a; 							\
	    xmax += a; 							\
	    nfound = 0;     						\
	    value = 0;     						\
	    for (iy=iymin; iy<=iymax; ++iy) {				\
		ptrIn = (iotype *) srcimg[iy];  			\
		for (ix=ixmin; ix<=ixmax; ++ix) {			\
		  value += ptrIn[ix];		  			\
		  ++nfound;			  			\
		}				  			\
	    } /* end of iy loop */					\
									\
	    if (nfound) 						\
		value /= nfound;					\
	    else							\
	        value = constant;					\
									\
	    /* prepare for next loop */					\
	    width--; 							\
	    ixmin = xmin;		     				\
	    ixmax = xmax;     						\
	    if (flags & FLG_C_NOT_ZERO) {				\
		ymin += c; 						\
		ymax += c; 						\
		iymin = ymin;     					\
		iymax = ymax;     					\
		if (iymin < minline)	iymin = minline;		\
	       	if (iymax >= maxline)	iymax = maxline;		\
		if (iymax > iymin)	iymax--;			\
	    } 								\
	    if (ixmin < 0)		ixmin = 0;			\
	    if (ixmax >= maxpixl)	ixmax = maxpixl;		\
	    if (ixmax > ixmin)		ixmax--;			\
	    *outp++ = value;						\
	}/* end of line loop */						\
									\
	/* before leaving, update bounding rect for next line */	\
	if (flags & FLG_B_NOT_ZERO) {					\
	   pvtband->left_br.xmin += b;					\
	   pvtband->left_br.xmax += b;					\
	}								\
	if (flags & FLG_D_NOT_ZERO) {					\
	   pvtband->left_br.ymin += d;					\
	   pvtband->left_br.ymax += d;					\
	}								

#define DO_AAGL(funcname, iotype, valtype, CONST)			\
static void								\
funcname (OUTP,srcimg,width,ped,pvtband)				\
    register pointer OUTP;						\
    register pointer *srcimg;						\
    register int width;							\
    peDefPtr  ped;							\
    mpAABandPtr pvtband;						\
{									\
    pGeomDefPtr	pedpvt = (pGeomDefPtr)ped->elemPvt; 			\
									\
    /* Mapping coefficients */						\
    double a = pedpvt->coeffs[0]; 					\
    double b = pedpvt->coeffs[1];					\
    double c = pedpvt->coeffs[2]; 					\
    double d = pedpvt->coeffs[3];					\
    CARD32 flags = pvtband->flags;					\
									\
    /* These variables describe the limits of the bounding rectangle */	\
    double xmin = pvtband->left_br.xmin;				\
    double ymin = pvtband->left_br.ymin;				\
    double xmax = pvtband->left_br.xmax;				\
    double ymax = pvtband->left_br.ymax;				\
    register int ixmin,iymin,ixmax,iymax;				\
									\
    /* loop variables for roaming through the bounding rectangle */	\
    register int ix,iy;							\
									\
    /* variables which describe available input data (for clipping) */	\
    register int maxpixl = pvtband->in_width - 1;			\
    register int minline = pvtband->lo_src_avail;			\
    register int maxline = pvtband->hi_src_avail;			\
									\
    /* cast of constant, output and input pointers to correct type */	\
    register iotype constant = (iotype) pvtband->CONST;			\
    register iotype *outp = (iotype *) OUTP;				\
    register iotype *ptrIn;						\
									\
    register valtype value;						\
    register int nfound;						\
									\
    inscrutable_compiler(funcname, iotype, valtype, CONST)		\
}

DO_AAGL	(AAGL_R, RealPixel, RealPixel, flt_constant)
DO_AAGL	(AAGL_B, BytePixel, QuadPixel, int_constant)
DO_AAGL	(AAGL_P, PairPixel, QuadPixel, int_constant)
DO_AAGL	(AAGL_Q, QuadPixel, QuadPixel, int_constant)

#if XIE_FULL
/************************************************************************/
/*			Gaussian Routines				*/
/************************************************************************/
/* NOTES:
** this is so slow, we don't try to do a special scale routine.
** should use #include <math.h>?
** should consider using float (e.g. fexp()) but tends to be machine specific.
** should generate pow(2,K) as (1 << floor(k)) + table[fraction(K)].
** (pow(2,K) is half the speed of exp() which defeats the purpose).
** issue of whether to pick points within a radius? or to use a constant
** ...size box of size 2 * radius ?

	(((uv2 = ((ix-xcen)*(ix-xcen)+(iy-ycen)*(iy-ycen))) <= rad2 )	\
		? ((double)(P)) * (simple				\
			?  pow(2.0, sigma * uv2)			\
			: exp(sigma * uv2))				\
		: 0.0 )
*/

#if !defined(exp) && !defined(pow)
extern double exp(), pow();
#endif


#define gauss_loop(funcname, iotype, valtype, CONST)			\
									\
	/* loop through all the output pixels on this line */		\
	while ( width-- > 0 ) { 					\
									\
	    iymin = ycen - (radius-1);					\
	    iymax = iymin + (radius+radius-1);				\
	    if (iymin < minline)	iymin = minline;		\
	    if (iymax >= maxline)	iymax = maxline;		\
									\
	    ixmin = xcen - (radius - 1);				\
	    ixmax = ixmin + (radius + radius - 1);			\
	    if (ixmin < 0)		ixmin = 0;			\
	    if (ixmax >= maxpixl)	ixmax = maxpixl;		\
									\
	    nfound = 0;     						\
	    value = 0.;     						\
	    for (iy = iymin; iy <= iymax; ++iy) {			\
		ptrIn = (iotype *) srcimg[iy];  			\
		for (ix = ixmin; ix <= ixmax; ++ix) {			\
		  uv2 = ((ix-xcen)*(ix-xcen)+(iy-ycen)*(iy-ycen));	\
		  value += ((double)(ptrIn[ix])) * (simple		\
				? pow(2.0, sigma * uv2)			\
				: exp(sigma * uv2));			\
		  ++nfound;			  			\
		}				  			\
	    } /* end of iy loop */					\
									\
	    if (nfound) { 						\
		value *= tkpvt->normalize; /* xxx clip */		\
		*outp++ = value < levels ? value : levels;		\
	    } else							\
	        *outp++ = constant;					\
									\
    	    xcen += a;							\
	    ycen += c;							\
	}/* end of line loop */						\
									\
	/* before leaving, update bounding rect for next line */	\
	if (flags & FLG_B_NOT_ZERO) {					\
	   pvtband->left_br.xmin += b;					\
	   pvtband->left_br.xmax += b;					\
	}								\
	if (flags & FLG_D_NOT_ZERO) {					\
	   pvtband->left_br.ymin += d;					\
	   pvtband->left_br.ymax += d;					\
	}								

#define DO_GAGL(funcname, iotype, valtype, CONST)			\
static void								\
funcname (OUTP,srcimg,width,ped,pvtband)				\
    register pointer OUTP;						\
    register pointer *srcimg;						\
    register int width;							\
    peDefPtr  ped;							\
    mpAABandPtr pvtband;						\
{									\
    pGeomDefPtr	pedpvt = (pGeomDefPtr)ped->elemPvt; 			\
									\
    /* Mapping coefficients */						\
    double a = pedpvt->coeffs[0]; 					\
    double b = pedpvt->coeffs[1];					\
    double c = pedpvt->coeffs[2]; 					\
    double d = pedpvt->coeffs[3];					\
    CARD32 flags = pvtband->flags;					\
									\
    register double xcen = pvtband->yOut * b + pedpvt->coeffs[4];	\
    register double ycen = pvtband->yOut * d + pedpvt->coeffs[5];	\
    register int ixmin,iymin,ixmax,iymax;				\
									\
    /* loop variables for roaming through the bounding rectangle */	\
    register int ix,iy;							\
									\
    /* variables which describe available input data (for clipping) */	\
    register int maxpixl = pvtband->in_width - 1;			\
    register int minline = pvtband->lo_src_avail;			\
    register int maxline = pvtband->hi_src_avail;			\
									\
    /* cast of constant, output and input pointers to correct type */	\
    register iotype constant = (iotype) pvtband->CONST;			\
    register iotype *outp = (iotype *) OUTP;				\
    register iotype *ptrIn;						\
									\
    register double value, levels = pvtband->level_clip - 1;		\
    register int nfound;						\
    pTecGeomGaussianDefPtr tkpvt = (pTecGeomGaussianDefPtr)ped->techPvt;\
    int			  simple = tkpvt->simple;			\
    int			  radius = tkpvt->radius;			\
    double		   sigma = (simple ? -1.0 : -0.5) /		\
					(tkpvt->sigma * tkpvt->sigma);	\
    register double	     uv2;					\
									\
    gauss_loop(funcname, iotype, valtype, CONST)			\
}

DO_GAGL	(GAGL_R, RealPixel, RealPixel, flt_constant)
DO_GAGL	(GAGL_B, BytePixel, QuadPixel, int_constant)
DO_GAGL	(GAGL_P, PairPixel, QuadPixel, int_constant)
DO_GAGL	(GAGL_Q, QuadPixel, QuadPixel, int_constant)
#endif

/**********************************************************************/
/* end module mpgeomaa.c */
