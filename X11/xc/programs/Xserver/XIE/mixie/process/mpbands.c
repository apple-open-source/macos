/* $Xorg: mpbands.c,v 1.4 2001/02/09 02:04:30 xorgcvs Exp $ */
/**** module mpbands.c ****/
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
  
	mpbands.c -- DDXIE BandSelect element
  
	Robert NC Shelley -- AGE Logic, Inc. September, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/process/mpbands.c,v 3.5 2001/12/14 19:58:43 dawes Exp $ */

#define _XIEC_MPBANDS
#define _XIEC_PBANDS

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


/* routines referenced by other DDXIE modules
 */
int	miAnalyzeBandSel();

/* routines used internal to this module
 */
static int CreateBandSel();
static int InitializeBandSel();
static int ActivateBandSel();
static int ResetBandSel();
static int DestroyBandSel();

/* DDXIE BandSelect entry points
 */
static ddElemVecRec BandSelVec = {
  CreateBandSel,
  InitializeBandSel,
  ActivateBandSel,
  (xieIntProc)NULL,
  ResetBandSel,
  DestroyBandSel
  };


/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeBandSel(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* for now just stash our entry point vector in the peDef */
  ped->ddVec = BandSelVec;

  return(TRUE);
}                               /* end miAnalyzeBandSel */

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateBandSel(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* attach an execution context to the photo element definition */
  return MakePETex(flo, ped, NO_PRIVATE, NO_SYNC, NO_SYNC);
}                               /* end CreateBandSel */


/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeBandSel(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloBandSelect *raw = (xieFloBandSelect *)ped->elemRaw;

  if(raw->bandNumber)
    return(InitReceptor(flo,ped,&ped->peTex->receptor[SRCtag],NO_DATAMAP,1, 
			(bandMsk)(1<<raw->bandNumber),NO_BANDS) &&
	   InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE));
  else
    return(InitReceptor(flo,ped,&ped->peTex->receptor[SRCtag],NO_DATAMAP,1,
			NO_BANDS,(bandMsk)1));
}                               /* end InitializeBandSel */


/*------------------------------------------------------------------------
--------------------------- pass some input data -------------------------
------------------------------------------------------------------------*/
static int ActivateBandSel(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  xieFloBandSelect *raw  = (xieFloBandSelect*)ped->elemRaw;
  bandPtr           sbnd = &pet->receptor->band[raw->bandNumber];
  bandPtr           dbnd =  pet->emitter;
  
  /* pass the chosen receptor band to our output band
   */
  if(GetCurrentSrc(flo,pet,sbnd)) {
    do {
      /* pass a clone of the current src strip downstream
       */
      if(!PassStrip(flo,pet,dbnd,sbnd->strip))
	return(FALSE);
    } while(GetSrc(flo,pet,sbnd,sbnd->maxLocal,FLUSH));

    FreeData(flo,pet,sbnd,sbnd->maxLocal);
  }
  return(TRUE);
}                               /* end ActivateBandSel */


/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetBandSel(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  xieFloBandSelect *raw = (xieFloBandSelect *)ped->elemRaw;

  if(raw->bandNumber) {
    ResetReceptors(ped);
    ResetEmitter(ped);
  }  
  return(TRUE);
}                               /* end ResetBandSel */


/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyBandSel(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* get rid of the peTex structure  */
  ped->peTex = (peTexPtr) XieFree(ped->peTex);
  
  /* zap this element's entry point vector */
  ped->ddVec.create     = (xieIntProc)NULL;
  ped->ddVec.initialize = (xieIntProc)NULL;
  ped->ddVec.activate   = (xieIntProc)NULL;
  ped->ddVec.reset      = (xieIntProc)NULL;
  ped->ddVec.destroy    = (xieIntProc)NULL;
  
  return(TRUE);
}                               /* end DestroyBandSel */

/* end module mpbands.c */
