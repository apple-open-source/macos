/* $Xorg: mpdither.c,v 1.4 2001/02/09 02:04:31 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module mpdither.c ****/
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
  
	mpdither.c -- DDXIE dither element
  
	Larry Hare -- AGE Logic, Inc. May, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mpdither.c,v 3.5 2001/12/14 19:58:45 dawes Exp $ */


#define _XIEC_MPDITHER
#define _XIEC_PDITHER

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
#include <technq.h>
#include <memory.h>

/*
 *  routines referenced by other DDXIE modules
 */
int	miAnalyzeDither();

/*
 *  routines used internal to this module
 */
static int CreateDitherErrorDiffusion(), CreateDitherOrdered();
static int InitializeDitherErrorDiffusion(), InitializeDitherOrdered();
static int ActivateDitherErrorDiffusion(), ActivateDitherOrdered();
static int ResetDitherErrorDiffusion(), ResetDitherOrdered();
static int DestroyDither();


typedef float DitherFloat;	/* see xiemd.h */

						/* Diffusion Dither Locals */
typedef struct _mpditherEDdef {
  void		(*action) ();
  DitherFloat	*previous;
  DitherFloat	*current;
  DitherFloat	range;
  DitherFloat	range1over;
  DitherFloat	round;
  INT32		width;
#if defined(SF_DITHER)
  CARD32        shift;
  INT32         irange;
  INT32         iround;
#endif
} mpDitherEDDefRec, *mpDitherEDDefPtr;

static void EdDitherQb(), EdDitherPb(), EdDitherBb(), EdDitherbb();
static void EdDitherQB(), EdDitherPB(), EdDitherBB();
static void EdDitherQP(), EdDitherPP();
static void EdDitherQQ();

						/* Ordered Dither Locals */
typedef struct _mpditherOrddef {
  void		(*action) ();
  CARD32	*matrix;
  CARD32	ncol;
  CARD32	nrow;	
  CARD32	shift;
  CARD32	mult;
  CARD32	width;
} mpDitherOrdDefRec, *mpDitherOrdDefPtr;

static Bool SetupOrderMatrix();

static void OrdDitherQb(), OrdDitherPb(), OrdDitherBb();
static void OrdDitherQB(), OrdDitherPB(), OrdDitherBB();
static void OrdDitherQP(), OrdDitherPP();
static void OrdDitherQQ();



/*
 * DDXIE Dither entry points
 */
/* static		Testing Hack.  See mpcnst.c */
ddElemVecRec DitherErrorDiffusion = {
	CreateDitherErrorDiffusion,
	InitializeDitherErrorDiffusion,
	ActivateDitherErrorDiffusion,
	(xieIntProc)NULL,
	ResetDitherErrorDiffusion,
	DestroyDither
};

static ddElemVecRec DitherOrdered = {
	CreateDitherOrdered,
	InitializeDitherOrdered,
	ActivateDitherOrdered,
	(xieIntProc)NULL,
	ResetDitherOrdered,
	DestroyDither
};

/*------------------------------------------------------------------------
------------------------  fill in the vector  ---------------------------
------------------------------------------------------------------------*/
int miAnalyzeDither(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{

    switch(ped->techVec->number) {
	case	xieValDitherErrorDiffusion:
		ped->ddVec = DitherErrorDiffusion;
		break;
	case	xieValDitherOrdered:
		ped->ddVec = DitherOrdered;
		break;
    }

    return TRUE;
}

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateDitherErrorDiffusion(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    int auxsize = xieValMaxBands * sizeof(mpDitherEDDefRec);

    return MakePETex(flo,ped,auxsize,NO_SYNC,NO_SYNC);
}

static int CreateDitherOrdered(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    int auxsize = xieValMaxBands * sizeof(mpDitherOrdDefRec);

    return MakePETex(flo,ped,auxsize,NO_SYNC,NO_SYNC);
}

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeDitherErrorDiffusion(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
    peTexPtr  pet = ped->peTex;
    mpDitherEDDefPtr pvt = (mpDitherEDDefPtr) (pet->private);
    bandMsk bmask = ((xieFloDither *) (ped->elemRaw))->bandMask;
    bandPtr oband;
    bandPtr iband;
    int band, nbands;
    void (*action) () = (void (*)()) 0;

    oband = &(pet->emitter[0]);
    iband = &(pet->receptor[SRCtag].band[0]);
    nbands = pet->receptor[SRCtag].inFlo->bands;
    for (band = 0; band < nbands; band++, pvt++, iband++, oband++) {

	if ((bmask & (1<<band)) == 0) continue;

	switch (oband->format->class) {
	case	QUAD_PIXEL:
		switch (iband->format->class) {
		case	QUAD_PIXEL:  action = EdDitherQQ; break;
		default:	     break;
		}
		break;
	case	PAIR_PIXEL:
		switch (iband->format->class) {
		case	QUAD_PIXEL:  action = EdDitherQP; break;
		case	PAIR_PIXEL:  action = EdDitherPP; break;
		default:	     break;
		}
		break;
	case	BYTE_PIXEL:
		switch (iband->format->class) {
		case	QUAD_PIXEL:  action = EdDitherQB; break;
		case	PAIR_PIXEL:  action = EdDitherPB; break;
		case	BYTE_PIXEL:  action = EdDitherBB; break;
		default:	     break;
		}
		break;
	case	BIT_PIXEL:
		switch (iband->format->class) {
		case	QUAD_PIXEL:  action = EdDitherQb; break;
		case	PAIR_PIXEL:  action = EdDitherPb; break;
		case	BYTE_PIXEL:  action = EdDitherBb; break;
		case	BIT_PIXEL:   action = EdDitherbb; break;
		default:	     break;
		}
		break;
	default:
		break;
	}
	if (!action)
	    ImplementationError(flo, ped, return(FALSE));

	pvt->action = action;
	pvt->width = iband->format->width;
	pvt->range = (iband->format->levels-1.0)/(oband->format->levels-1.0);
	pvt->round = pvt->range/((DitherFloat)2.0);
	pvt->range1over = ((DitherFloat) 1.0) / pvt->range;

	if(pvt->range == 1.0) {
	    bmask &= ~(1<<band);
	    continue;
	}
#if defined(SF_DITHER)
        pvt->shift = (iband->format->class == PAIR_PIXEL ? 11 : 19);
        pvt->irange = pvt->range * (1<<pvt->shift);
        pvt->iround = pvt->round * (1<<pvt->shift);
#endif

	if (ped->techVec->number == xieValDitherErrorDiffusion) {
	    /* Use XieCalloc to force DitherFloat's to 0.0 */
	    int auxsize = (pvt->width + 2) * sizeof(DitherFloat);
	    if((!(pvt->previous = (DitherFloat *) XieCalloc(auxsize))) ||
	       (!(pvt->current  = (DitherFloat *) XieCalloc(auxsize))))
		    AllocError(flo, ped, return(FALSE));
	}
    }
    return
        InitReceptor(flo,ped,&pet->receptor[SRCtag],NO_DATAMAP,1,bmask,~bmask)
	&& InitEmitter(flo,ped,NO_DATAMAP,NO_INPLACE);

}

static int InitializeDitherOrdered(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    peTexPtr  pet = ped->peTex;
    mpDitherOrdDefPtr pvt = (mpDitherOrdDefPtr) pet->private;
    xieFloDither *elem = (xieFloDither *) (ped->elemRaw);
    CARD8 thresh = ((xieTecDitherOrdered *) (&elem[1]))->thresholdOrder;
    bandMsk bmask = elem->bandMask;
    bandPtr oband;
    bandPtr iband;
    int band, nbands;
    void (*action) () = (void(*)()) 0;

    oband = &(pet->emitter[0]);
    iband = &(pet->receptor[SRCtag].band[0]);
    nbands = pet->receptor[SRCtag].inFlo->bands;
    for (band = 0; band < nbands; band++, pvt++, iband++, oband++) {

	if ((bmask & (1<<band)) == 0) continue;

	switch (oband->format->class) {
	case	QUAD_PIXEL:
		switch (iband->format->class) {
		case	QUAD_PIXEL:  action = OrdDitherQQ; break;
		default:	     break;
		}
		break;
	case	PAIR_PIXEL:
		switch (iband->format->class) {
		case	QUAD_PIXEL:  action = OrdDitherQP; break;
		case	PAIR_PIXEL:  action = OrdDitherPP; break;
		default:	     break;
		}
		break;
	case	BYTE_PIXEL:
		switch (iband->format->class) {
		case	QUAD_PIXEL:  action = OrdDitherQB; break;
		case	PAIR_PIXEL:  action = OrdDitherPB; break;
		case	BYTE_PIXEL:  action = OrdDitherBB; break;
		default:	     break;
		}
		break;
	case	BIT_PIXEL:
		switch (iband->format->class) {
		case	QUAD_PIXEL:  action = OrdDitherQb; break;
		case	PAIR_PIXEL:  action = OrdDitherPb; break;
		case	BYTE_PIXEL:  action = OrdDitherBb; break;
		default:	     break;
		}
		break;
	default:
		break;
	}
	if (!action)
	    ImplementationError(flo, ped, return(FALSE));

	pvt->action = action;
	pvt->width = iband->format->width;

	switch (iband->format->class) {
	case	QUAD_PIXEL:  pvt->shift=6; break;
	case	PAIR_PIXEL:  pvt->shift=14; break;
	case	BYTE_PIXEL:  pvt->shift=22; break;
	}
	pvt->mult = ((1<<pvt->shift) * (oband->format->levels-1.0)) / 
					(iband->format->levels-1.0);

	if(pvt->mult == (1<<pvt->shift)) {
	    bmask &= ~(1<<band);
	    continue;
	}

	(void) SetupOrderMatrix(pvt, (CARD32) thresh);

	if (!pvt->matrix)
	    AllocError(flo, ped, return(FALSE));
    }
    return
        InitReceptor(flo,ped,&pet->receptor[SRCtag],NO_DATAMAP,1,bmask,~bmask)
	&& InitEmitter(flo,ped,NO_DATAMAP,NO_INPLACE);

}


static CARD8 ddmtrx1[]  = { 1, 0, 1, 0,
			    0, 1, 0, 1,
			    1, 0, 1, 0,
			    0, 1, 0, 1
			  };

static CARD8 ddmtrx2[]  = { 1, 2, 1, 2,
			    3, 0, 3, 0,
			    1, 2, 1, 2,
			    3, 0, 3, 0
			  };

static CARD8 ddmtrx3[]  = { 1, 6, 0, 7,
			    5, 3, 4, 2,
			    0, 7, 1, 6,
			    4, 2, 5, 3
			  };

static CARD8 ddmtrx4[]  = {  1, 15,  2, 12,
			     9,  5, 10,  6,
			     3, 13,  0, 14,
			    11,  7,  8,  4
			  };

static CARD8 ddmtrx5[]  = {  1, 28,  6, 26,  0, 29,  7, 27,
			    17,  9, 22, 14, 16,  8, 23, 15,
			     5, 25,  3, 30,  4, 24,  2, 31,
			    21, 13, 19, 11, 20, 12, 18, 10,
			  };

static CARD8 ddmtrx6[]  = {  1, 59, 15, 55,  2, 56, 12, 52,
			    33, 17, 47, 31, 34, 18, 44, 28,
			     9, 49,  5, 63, 10, 50,  6, 60,
			    41, 25, 37, 21, 42, 26, 38, 22, 
			     3, 57, 13, 53,  0, 58, 14, 54,
			    35, 19, 45, 29, 32, 16, 46, 30,
			    11, 51,  7, 61,  8, 48,  4, 62,
			    43, 27, 39, 23, 40, 24, 36, 20
			  };

static CARD8 ddmtrx7[]  = {
  1, 116,  28, 108,   6, 114,  26, 106,   0, 117,  29, 109,   7, 115,  27, 107,
 65,  33,  92,  60,  70,  38,  90,  58,  64,  32,  93,  61,  71,  39,  91,  59,
 17,  97,   9, 124,  22, 102,  14, 122,  16,  96,   8, 125,  23, 103,  15, 123,
 81,  49,  73,  41,  86,  54,  78,  46,  80,  48,  72,  40,  87,  55,  79,  47,
  5, 113,  25, 105,   3, 118,  30, 110,   4, 112,  24, 104,   2, 119,  31, 111,
 69,  37,  89,  57,  67,  35,  94,  62,  68,  36,  88,  56,  66,  34,  95,  63,
 21, 101,  13, 121,  19,  99,  11, 126,  20, 100,  12, 120,  18,  98,  10, 127,
 85,  53,  77,  45,  83,  51,  75,  43,  84,  52,   76, 44,  82,  50,  74,  42,
};

static CARD8 ddmtrx8[]  = {
  1, 235,  49, 219,  15, 231,  55, 215,   2, 232,  56, 216,  12, 228,  52, 212,
129,  65, 187, 123, 143,  79, 183, 119, 130,  66, 184, 120, 140,  76, 180, 116,
 33, 193,  17, 251,  47, 207,  31, 247,  34, 194,  18, 248,  44, 204,  28, 244,
161,  97, 145,  81, 175, 111, 159,  95, 162,  98, 146,  82, 172, 108, 156,  92,
  9, 225,  49, 209,   5, 239,  63, 223,  10, 226,  50, 210,   6, 236,  60, 220,
137,  73, 177, 113, 133,  69, 191, 127, 138,  74, 178, 114, 134,  70, 188, 124,
 41, 201,  25, 241,  37, 197,  21, 255,  42, 202,  26, 242,  38, 198,  22, 252,
169, 105, 153,  89, 165, 101, 149,  85, 170, 106, 154,  90, 166, 102, 150,  86,
  3, 233,  57, 217,  13, 229,  53, 213,   0, 234,  58, 218,  14, 230,  54, 214,
131,  67, 185, 121, 141,  77, 181, 117, 128,  64, 186, 122, 142,  78, 182, 118,
 35, 195,  19, 249,  45, 205,  29, 245,  32, 192,  16, 250,  46, 206,  30, 246,
163,  99, 147,  83, 173, 109, 157,  93, 160,  96, 144,  80, 174, 110, 158,  94,
 11, 227,  51, 211,   7, 237,  61, 221,   8, 224,  48, 208,   4, 238,  62, 222,
139,  75, 179, 115, 135,  71, 189, 127, 136,  72, 176, 112, 132,  68, 190, 126,
 43, 203,  27, 243,  39, 199,  23, 253,  40, 200,  24, 240,  36, 196,  20, 254,
171, 107, 155,  91, 167, 103, 151,  87, 168, 104, 152,  88, 164, 100, 148,  84
};



static Bool
SetupOrderMatrix(pvt, threshold)
    mpDitherOrdDefPtr pvt;
    CARD32 threshold;
{
    CARD32 shift = 1 << pvt->shift;
    CARD32 *mtrx;
    CARD8  *srcmtrx;
    CARD32 nr, nc, nt;
    INT32  m;

    switch(threshold) {
	case 1:		srcmtrx = ddmtrx1; nr = 4; nc = 4; nt = 2; break; 
	case 2:		srcmtrx = ddmtrx2; nr = 4; nc = 4; nt = 4; break; 
	case 3:		srcmtrx = ddmtrx3; nr = 4; nc = 4; nt = 8; break; 
	case 4:		srcmtrx = ddmtrx4; nr = 4; nc = 4; nt = 16; break; 
	case 5:		srcmtrx = ddmtrx5; nr = 4; nc = 8; nt = 32; break; 
	case 6:		srcmtrx = ddmtrx6; nr = 8; nc = 8; nt = 64; break; 
	case 7:		srcmtrx = ddmtrx7; nr = 8; nc =16; nt = 128; break;
	default:
	case 8:		srcmtrx = ddmtrx8; nr =16; nc =16; nt = 256; break; 
    }

    if (!(mtrx = (CARD32 *)XieMalloc(nr * nc * sizeof(CARD32))))
	return FALSE;

    for (m = nr * nc -1; m >= 0 ; m--)
	mtrx[m] = ((CARD32)srcmtrx[m] * shift + (shift>>1)) / nt;

    /*
    ** This would be a good place to spin the matrix to avoid
    ** aligning the dither pattern on triple band images. 
    */

    pvt->ncol = nc;
    pvt->nrow = nr;
    pvt->matrix = mtrx;
    return TRUE;
}

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/


static int ActivateDitherErrorDiffusion(flo,ped,pet)
    floDefPtr flo;
    peDefPtr  ped;
    peTexPtr  pet;
{
    mpDitherEDDefPtr pvt = (mpDitherEDDefPtr) pet->private;
    bandPtr oband = &(pet->emitter[0]);
    bandPtr iband = &(pet->receptor[SRCtag].band[0]);
    int band, nbands = pet->receptor[SRCtag].inFlo->bands;

    for(band = 0; band < nbands; band++, pvt++, iband++, oband++) {
	register pointer inp, outp;

	if (!(inp  = GetCurrentSrc(flo,pet,iband)) ||
	    !(outp = GetCurrentDst(flo,pet,oband))) continue;

	do {
		(*(pvt->action)) (inp, outp, pvt);

		{   /* Swap error buffers */
		    DitherFloat *curr = pvt->current;
		    curr = pvt->current;
		    pvt->current  = pvt->previous;
		    pvt->previous = curr;
		}
		inp  = GetNextSrc(flo,pet,iband,FLUSH);
		outp = GetNextDst(flo,pet,oband,FLUSH);

	} while (inp && outp) ;

	FreeData(flo, pet, iband, iband->current);
    }
    return TRUE;
}

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/

static int ActivateDitherOrdered(flo,ped,pet)
    floDefPtr flo;
    peDefPtr  ped;
    peTexPtr  pet;
{
    mpDitherOrdDefPtr pvt = (mpDitherOrdDefPtr) pet->private;
    bandPtr oband = &(pet->emitter[0]);
    bandPtr iband = &(pet->receptor[SRCtag].band[0]);
    int band, nbands = pet->receptor[SRCtag].inFlo->bands;

    for(band = 0; band < nbands; band++, pvt++, iband++, oband++) {
	register pointer inp, outp;

	if (!(inp  = GetCurrentSrc(flo,pet,iband)) ||
	    !(outp = GetCurrentDst(flo,pet,oband))) continue;

	do {
		(*(pvt->action)) (inp, outp, pvt, oband->current);

		inp  = GetNextSrc(flo,pet,iband,FLUSH);
		outp = GetNextDst(flo,pet,oband,FLUSH);

	} while (inp && outp) ;

	FreeData(flo, pet, iband, iband->current);
    }
    return TRUE;
}

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetDitherErrorDiffusion(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    mpDitherEDDefPtr pvt = (mpDitherEDDefPtr) ped->peTex->private;
    int band;

    /* free any dynamic private data */
    for (band = 0 ; band < xieValMaxBands ; band++, pvt++) {
	pvt->width = 0;
	pvt->action = 0;
	if (pvt->previous)
		pvt->previous = (DitherFloat *) XieFree(pvt->previous);
	if (pvt->current)
		pvt->current  = (DitherFloat *) XieFree(pvt->current);
    }

    ResetReceptors(ped);
    ResetEmitter(ped);
    return TRUE;
}

static int ResetDitherOrdered(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    mpDitherOrdDefPtr pvt = (mpDitherOrdDefPtr) ped->peTex->private;
    int band;

    /* free any dynamic private data */
    for (band = 0 ; band < xieValMaxBands ; band++, pvt++) {
	pvt->width = 0;
	pvt->action = 0;
	if (pvt->matrix)
	    pvt->matrix = (CARD32 *) XieFree(pvt->matrix);
    }

    ResetReceptors(ped);
    ResetEmitter(ped);
    return TRUE;
}

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyDither(flo,ped)
    floDefPtr flo;
    peDefPtr  ped;
{
    ped->peTex = (peTexPtr) XieFree(ped->peTex);

    /* zap this element's entry point vector */
    ped->ddVec.create     = (xieIntProc)NULL;
    ped->ddVec.initialize = (xieIntProc)NULL;
    ped->ddVec.activate   = (xieIntProc)NULL;
    ped->ddVec.reset      = (xieIntProc)NULL;
    ped->ddVec.destroy    = (xieIntProc)NULL;

    return TRUE;
}

/*------------------------------------------------------------------------
------------------------------- Macro Mania ------------------------------
------------------------------------------------------------------------*/

/*
** The only legal bit to bit dither is 2 levels to 1 level.
*/

static void EdDitherbb(inp,outp,pvt)
	pointer inp; pointer outp; mpDitherEDDefPtr pvt;
{
	bzero((char *)outp, (pvt->width+7)>>3);
}

/*
** Quads, Pairs, and Bytes can be dithered to bits.  Since this code is
** so slow anyway, don't get too excited about optimizing the bit output.
*/

#define MakeEdBit(fn_name,itype,otype)					\
static void fn_name(INP,OUTP,pvt) 					\
	pointer INP; pointer OUTP; mpDitherEDDefPtr pvt;		\
{									\
	register itype *inp = (itype *) INP;				\
	register LogInt *outp = (LogInt *) OUTP;			\
	register unsigned int actual;					\
	register DitherFloat range = pvt->range;			\
	register DitherFloat round = pvt->round;			\
	register DitherFloat *prev = pvt->previous;			\
	register DitherFloat *curr = pvt->current;			\
	register DitherFloat range1over = pvt->range1over;		\
	register DitherFloat current = *curr, desire;			\
	register int ix, bw = pvt->width;				\
	bzero((char *)outp, (bw+7)>>3);					\
	for (ix = 0; ix < bw; ix++) {					\
	    desire = inp[ix] + ((DitherFloat) .4375 * current) +	\
			    ((DitherFloat) .0625 * *(prev+0)) +		\
			    ((DitherFloat) .3125 * *(prev+1)) +		\
			    ((DitherFloat) .1875 * *(prev+2)) ;		\
	    prev++;							\
	    actual = (desire+round)*range1over;				\
	    if (actual) { 						\
	        LOG_setbit(outp,ix);					\
	        *++curr = current = desire - actual*range;		\
	    } else							\
	        *++curr = current = desire;				\
	}								\
}

#if defined(SF_DITHER)
#define MakeEdBitI(fn_name,itype,otype)                                 \
static void fn_name(INP,OUTP,pvt)                                       \
        pointer INP; pointer OUTP; mpDitherEDDefPtr pvt;                \
{                                                                       \
        register itype *inp = (itype *) INP;                            \
        register LogInt *outp = (LogInt *) OUTP;                        \
        register unsigned int actual;                                   \
        register CARD32 shift = pvt->shift;                             \
        register INT32 range = pvt->irange;                             \
        register INT32 round = pvt->iround;                             \
        register INT32 *prev = (INT32 *) pvt->previous;                 \
        register INT32 *curr = (INT32 *) pvt->current;                  \
        register INT32 current = *curr, desire;                         \
        register int ix, bw = pvt->width;                               \
        bzero((char *)outp, (bw+7)>>3);                                 \
        for (ix = 0; ix < bw; ix++) {                                   \
            desire = (((CARD32) inp[ix]) << shift) +                    \
                                      ((( current  * 7) +               \
                                        (*(prev+0)    ) +               \
                                        (*(prev+1) * 5) +               \
                                        (*(prev+2) * 3) ) >> 4) ;       \
            prev++;                                                     \
            actual = (desire+round)/range;                              \
            if (actual) {                                               \
                LOG_setbit(outp,ix);                                    \
                *++curr = current = desire - actual*range;              \
            } else                                                      \
                *++curr = current = desire;                             \
        }                                                               \
}
#endif

/*
** All the other multitudes of combinations.
*/

#define MakeEdPix(fn_name,itype,otype)					\
static void fn_name(INP,OUTP,pvt) 					\
	pointer INP; pointer OUTP; mpDitherEDDefPtr pvt;		\
{									\
	register itype *inp = (itype *) INP;				\
	register otype *outp = (otype *) OUTP, actual;			\
	register DitherFloat range = pvt->range;			\
	register DitherFloat round = pvt->round;			\
	register DitherFloat *prev = pvt->previous;			\
	register DitherFloat *curr = pvt->current;			\
	register DitherFloat current = *curr, desire;			\
	register DitherFloat range1over = pvt->range1over;		\
	register int ix, bw = pvt->width;				\
	for (ix = 0; ix < bw; ix++) {					\
	    desire = inp[ix] + ((DitherFloat) .4375 * current) +	\
			    ((DitherFloat) .0625 * *(prev+0)) +		\
			    ((DitherFloat) .3125 * *(prev+1)) +		\
			    ((DitherFloat) .1875 * *(prev+2)) ;		\
	    prev++;							\
	    *outp++ = actual  = (desire+round)*range1over;		\
	    *++curr = current = (desire - actual*range);		\
	}								\
}

#if defined(SF_DITHER)
        /* NOTE:  has <<, /, then * which means many cycles.
        ** A better design would use a *, >>, *.  The last * might
        ** be a <<, and the first * on mips would pipeline with the
        ** *(prev+N) calculations.  This should double or triple the
        ** speed for many of these architectures.
        */
#define MakeEdPixI(fn_name,itype,otype)                                 \
static void fn_name(INP,OUTP,pvt)                                       \
        pointer INP; pointer OUTP; mpDitherEDDefPtr pvt;                \
{                                                                       \
        register itype *inp = (itype *) INP;                            \
        register otype *outp = (otype *) OUTP;                          \
        register CARD32 actual;                                         \
        register CARD32 shift = pvt->shift;                             \
        /* ?? replace divide by range with multiply and >> ?? */        \
        register INT32 range = pvt->irange;                             \
        register INT32 round = pvt->iround;                             \
        register INT32 *prev = (INT32 *) pvt->previous;                 \
        register INT32 *curr = (INT32 *) pvt->current;                  \
        register INT32 current = *curr, desire;                         \
        register int ix, bw = pvt->width;                               \
        for (ix = 0; ix < bw; ix++) {                                   \
            desire = (((CARD32)inp[ix]) << shift) +                     \
                                      (((current   * 7) +               \
                                        (*(prev+0)    ) +               \
                                        (*(prev+1) * 5) +               \
                                        (*(prev+2) * 3) ) >> 4) ;       \
            prev++;                                                     \
            *outp++ = actual = (desire+round)/range;                    \
            *++curr = current = desire - actual * range;                \
        }                                                               \
}
#endif

#if defined(SF_DITHER)

MakeEdBit       (EdDitherQb,QuadPixel,BitPixel)
MakeEdBitI      (EdDitherPb,PairPixel,BitPixel)
MakeEdBitI      (EdDitherBb,BytePixel,BitPixel)
MakeEdPix       (EdDitherQB,QuadPixel,BytePixel)
MakeEdPixI      (EdDitherPB,PairPixel,BytePixel)
MakeEdPixI      (EdDitherBB,BytePixel,BytePixel)
MakeEdPix       (EdDitherQP,QuadPixel,PairPixel)
MakeEdPixI      (EdDitherPP,PairPixel,PairPixel)
MakeEdPix       (EdDitherQQ,QuadPixel,QuadPixel)

#else
MakeEdBit	(EdDitherQb,QuadPixel,BitPixel)
MakeEdBit	(EdDitherPb,PairPixel,BitPixel)
MakeEdBit	(EdDitherBb,BytePixel,BitPixel)

MakeEdPix	(EdDitherQB,QuadPixel,BytePixel)
MakeEdPix	(EdDitherPB,PairPixel,BytePixel)
MakeEdPix	(EdDitherBB,BytePixel,BytePixel)

MakeEdPix	(EdDitherQP,QuadPixel,PairPixel)
MakeEdPix	(EdDitherPP,PairPixel,PairPixel)

MakeEdPix	(EdDitherQQ,QuadPixel,QuadPixel)

#endif

/*------------------------------------------------------------------------
------------------------------- Macro Mania ------------------------------
------------------------------------------------------------------------*/


#define MakeOrdBit(fn_name,itype,otype)					\
static void fn_name(INP,OUTP,pvt,ycur)					\
	pointer INP; pointer OUTP; mpDitherOrdDefPtr pvt; int ycur;	\
{									\
	register itype *inp = (itype *) INP;				\
	register LogInt *outp = (LogInt *) OUTP;			\
	register CARD32 mult = pvt->mult;				\
	register CARD32 shift = pvt->shift;				\
	register CARD32 ncol = pvt->ncol;				\
	register CARD32 *mtrx;						\
	register CARD32 jcol, nw;					\
	register LogInt outval, M;					\
	register int bw = pvt->width;					\
	CARD32 nrow = pvt->nrow;					\
	mtrx = pvt->matrix + ((ycur & (nrow-1)) * ncol);		\
	jcol = ((ncol > nrow) && (ycur & nrow)) ? nrow : 0;		\
	for (ncol--, nw = (bw >> LOGSHIFT); nw > 0; nw--) {		\
	    for (M=LOGLEFT, outval = 0; M ; LOGRIGHT(M)) {		\
		register CARD32 value1, value2;				\
		value1 = (*inp++ * mult + mtrx[jcol++]) >> shift;	\
		value2 = (*inp++ * mult + mtrx[jcol++]) >> shift;	\
		if (value1) outval |= M;				\
		value1 = (*inp++ * mult + mtrx[jcol++]) >> shift;	\
		LOGRIGHT(M); if (value2) outval |= M;			\
		value2 = (*inp++ * mult + mtrx[jcol++]) >> shift;	\
		LOGRIGHT(M); if (value1) outval |= M;			\
	        jcol &= ncol;						\
		LOGRIGHT(M); if (value2) outval |= M;			\
	    }								\
	    *outp++ = outval;						\
	}								\
	if ((nw = (bw & LOGMASK))) {					\
	    for (M=LOGLEFT, outval = 0; nw ; nw--, LOGRIGHT(M)) {	\
		if ((mtrx[jcol++] + *inp++ * mult) >> shift)		\
		    outval |= M;					\
	        jcol &= ncol;						\
	    }								\
	    *outp = outval;						\
	}								\
}

#define MakeOrdPix(fn_name,itype,otype)					\
static void fn_name(INP,OUTP,pvt,ycur)					\
	pointer INP; pointer OUTP; mpDitherOrdDefPtr pvt; int ycur;	\
{									\
	register itype *inp = (itype *) INP;				\
	register otype *outp = (otype *) OUTP;				\
	register CARD32 mult = pvt->mult;				\
	register CARD32 shift = pvt->shift;				\
	register CARD32 ncol = pvt->ncol;				\
	register CARD32 *mtrx;						\
	register CARD32 jcol;						\
	register int bw = pvt->width;					\
	CARD32 nrow = pvt->nrow;					\
	mtrx = pvt->matrix + ((ycur & (nrow-1)) * ncol);		\
	jcol = ((ncol > nrow) && (ycur & nrow)) ? nrow : 0;		\
	for (ncol--, bw-- ; bw > 0; bw -= 4) {				\
	    /* unroll four times is all we can do safely */		\
	    register CARD32 value1, value2;				\
	    value1 = (*inp++ * mult + mtrx[jcol++]) >> shift;		\
	    value2 = (*inp++ * mult + mtrx[jcol++]) >> shift;		\
	    *outp++ = value1;						\
	    value1 = (*inp++ * mult + mtrx[jcol++]) >> shift;		\
	    *outp++ = value2;						\
	    value2 = (*inp++ * mult + mtrx[jcol++]) >> shift;		\
	    *outp++ = value1;						\
	    jcol &= ncol;						\
	    *outp++ = value2;						\
	}								\
	for ( ; bw >= 0; bw--)						\
	    *outp++ = (*inp++ * mult + mtrx[jcol++]) >> shift;		\
}

MakeOrdBit	(OrdDitherQb,QuadPixel,BitPixel)
MakeOrdBit	(OrdDitherPb,PairPixel,BitPixel)
MakeOrdBit	(OrdDitherBb,BytePixel,BitPixel)

MakeOrdPix	(OrdDitherQB,QuadPixel,BytePixel)
MakeOrdPix	(OrdDitherPB,PairPixel,BytePixel)
MakeOrdPix	(OrdDitherBB,BytePixel,BytePixel)

MakeOrdPix	(OrdDitherQP,QuadPixel,PairPixel)
MakeOrdPix	(OrdDitherPP,PairPixel,PairPixel)

MakeOrdPix	(OrdDitherQQ,QuadPixel,QuadPixel)

/* end module mpdither.c */
