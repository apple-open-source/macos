/* $Xorg: floman.c,v 1.4 2001/02/09 02:04:23 xorgcvs Exp $ */
/**** module floman.c ****/
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
  
	floman.c -- DDXIE photoflo manager
  
	Robert NC Shelley -- AGE Logic, Inc. May, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/control/floman.c,v 3.5 2001/12/14 19:58:17 dawes Exp $ */

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

/* routines used internal to this module
 */
static int flo_link(floDefPtr flo);
static int flo_startup(floDefPtr flo);
static int flo_resume(floDefPtr flo);
static int flo_shutdown(floDefPtr flo);
static int flo_destroy(floDefPtr flo);

/* DIXIE to DDXIE photoflo management entry points
 */
static floVecRec floManagerVec = {
  flo_link,
  flo_startup,
  flo_resume,
  flo_shutdown,
  flo_destroy
  };

/*------------------------------------------------------------------------
------------------------ Initialize Photoflo Manager ---------------------
------------------------------------------------------------------------*/
int InitFloManager(floDefPtr flo)
{
  /* plug in the DDXIE photoflo management vector */
  flo->floVec = &floManagerVec;

  return(TRUE);
}                               /* end InitFloManager */


/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
int MakePETex(
     floDefPtr   flo,
     peDefPtr    ped,
     CARD32   extend,
     Bool     inSync,
     Bool   bandSync)
{
  peTexPtr    pet;
  inFloPtr    inf;
  receptorPtr rcp;
  bandPtr     bnd;
  int b, i;
  
  /* attach an execution context to the photo element definition */
  if(!(pet = (peTexPtr) XieCalloc(sizeof(peTexRec) + (extend + 4) +
				  ped->inCnt * sizeof(receptorRec))))
    AllocError(flo,ped, return(FALSE));
  
  /* init the new peTex */
  ped->peTex    = pet;
  pet->peDef    = ped;
  pet->inSync   = inSync;
  pet->bandSync = bandSync;
  pet->outFlo   = &ped->outFlo;
  pet->receptor = (receptorPtr) &pet[1];
  for(b = 0; b < xieValMaxBands; b++) {
    bnd = &pet->emitter[b];
    bnd->band   = b;
    bnd->format = &ped->outFlo.format[b];
    ListInit(&bnd->stripLst);
  }
  /* init the new receptors */
  for(i = 0; i < ped->inCnt; i++) {
    inf = &ped->inFloLst[i];
    rcp = &pet->receptor[i];
    rcp->inFlo    = inf;
    for(b = 0; b < xieValMaxBands; b++) {
      bnd = &rcp->band[b];
      bnd->band      = b;
      bnd->isInput   = TRUE;
      bnd->receptor  = rcp;
      bnd->format    = &rcp->inFlo->format[b];
      ListInit(&bnd->stripLst);
    }
  }
  if(extend) {
    /* In case private structure has 'double', round up */
    unsigned char *ptr = (pointer) &pet->receptor[ped->inCnt];
    pet->private = (pointer)
	(((unsigned long)ptr + sizeof(double)-1) & -sizeof(double));
  }

  return(TRUE);
}                               /* end MakePETex */


/*------------------------------------------------------------------------
--------------------- prepare Receptors for execution --------------------
------------------------------------------------------------------------*/
Bool InitReceptors(
     floDefPtr flo,
     peDefPtr  ped,
     CARD32    mapSize,
     CARD32    threshold)
{
  receptorPtr rcp = ped->peTex->receptor;
  int i;
  
  /* initialize each receptor (1 per inFlo) */
  for(i = 0; i < ped->inCnt; ++rcp, ++i)
    if(!InitReceptor(flo,ped,rcp,mapSize,threshold,(CARD8)~0,(CARD8)0))
      return(FALSE);

  return(TRUE);
}                               /* end InitReceptors */


/*------------------------------------------------------------------------
--------------------- prepare Receptors for execution --------------------
------------------------------------------------------------------------*/
Bool InitReceptor(
     floDefPtr	 flo,
     peDefPtr	 ped,
     receptorPtr rcp,
     CARD32	 mapSize,
     CARD32	 threshold,
     unsigned	 process,
     unsigned	 bypass)
{
  bandPtr  bnd = rcp->band;
  int b, bands = rcp->inFlo->bands;
  
  /* bands to pass rather than process */
  rcp->bypass = rcp->inFlo->index == SRCt1 ? bypass : NO_BANDS;

  /* initialize each band */
  for(b = 0; b < bands; ++bnd, ++b)
    if(process & 1<<b)
      if(!InitBand(flo, ped, bnd, mapSize, threshold, NO_INPLACE))
	return(FALSE);

  return(TRUE);
}                               /* end InitReceptor */


/*------------------------------------------------------------------------
---------------------- prepare emitter for execution ---------------------
------------------------------------------------------------------------*/
Bool InitEmitter(
     floDefPtr flo,
     peDefPtr  ped,
     CARD32    mapSize,
     INT32     inPlace)
{
  peTexPtr pet = ped->peTex;
  int b;

  /* initialize the outFlo and emitter */
  ped->outFlo.active = NO_BANDS;
  ped->outFlo.ready  = NO_BANDS;

  /* initialize each band */
  for(b = 0; b < ped->outFlo.bands; b++) {
    if(pet->receptor[SRCt1].bypass & 1<<b)   continue;
      if(!InitBand(flo, ped, &pet->emitter[b], mapSize, (CARD32) 0, inPlace))
	return(FALSE);
  }
  return(TRUE);
}                               /* end InitEmitter */


/*------------------------------------------------------------------------
------------- prepare a receptor or emitter band for execution -----------
------------------------------------------------------------------------*/
Bool InitBand(
     floDefPtr	flo,
     peDefPtr	ped,
     bandPtr	bnd,
     CARD32	mapSize,
     CARD32	threshold,
     INT32	inPlace)
{
  bnd->threshold  = threshold;
  bnd->available  = 0;
  bnd->minGlobal  = 0;
  bnd->minLocal   = 0;
  bnd->current    = 0;
  bnd->maxLocal   = 0;
  bnd->maxGlobal  = 0;
  bnd->pitch      = IsCanonic(bnd->format->class) ? bnd->format->pitch>>3 : 1;
  bnd->strip      = NULL;
  bnd->data       = NULL;
  bnd->dataMap    = NULL;
  bnd->final      = FALSE;
  if((bnd->mapSize = mapSize) &&
     !(bnd->dataMap = (CARD8**)XieMalloc(mapSize * sizeof(CARD8*))))
    FloAllocError(flo,0,0, return(FALSE));

  if(bnd->isInput) {
    bnd->receptor->active |= 1<<bnd->band;
    bnd->receptor->attend |= 1<<bnd->band;
    bnd->inPlace = NULL;
    if(bnd->band == 0 || !bnd->receptor->band[0].replicate) {
      if(ped->flags.putData)
	++flo->floTex->imports;
      
      if(!bnd->receptor->admit)
	++ped->peTex->admissionCnt;
      bnd->receptor->admit |= 1<<bnd->band;
      
      if(bnd->replicate) {
	int b = 1;
	/* replicate band zero's format (into the phantom bands
	 * that will be sharing its data) and initialize them too
	 */
	do
	  if(bnd->replicate & 1<<b) {
	    *bnd[b].format = *bnd->format;
	     bnd[b].format->band = b;
	    InitBand(flo,ped,&bnd[b],NO_DATAMAP,threshold,NO_INPLACE);
	  } 
	while(++b < xieValMaxBands);
      }
    }
  } else { /* IsEmitter */
    bnd->inPlace = ((inPlace == NO_INPLACE)
		    ? NULL : &ped->peTex->receptor[inPlace].band[bnd->band]);
    ped->peTex->emitting |= 1<<bnd->band;
    if(ped->flags.getData) {
      ped->outFlo.active |= 1<<bnd->band;
      flo->floTex->exports++;
    }
  }
  return(TRUE);
}                               /* end InitBand */
    

/*------------------------------------------------------------------------
------------------- get rid of left over strips etc. ---------------------
------------------------------------------------------------------------*/
void ResetReceptors(peDefPtr ped)
{
  peTexPtr    pet = ped->peTex;
  receptorPtr rcp;
  int b,i;
  
  for(i = 0; i < ped->inCnt; i++) {
    for(rcp = &pet->receptor[i], b = 0; b < xieValMaxBands; ++b) {
      if(rcp->forward & 1<<b) {
	FreeStrips(&ped->outFlo.output[b]);
      }
      rcp->forward = NO_BANDS;
      ResetBand(&rcp->band[b]);
    }
    rcp->admit  = NO_BANDS;
    rcp->ready  = NO_BANDS;
    rcp->active = NO_BANDS;
    rcp->attend = NO_BANDS;
    rcp->bypass = NO_BANDS;
  }
}                               /* end ResetReceptors */


/*------------------------------------------------------------------------
------------------- get rid of left over strips etc. ---------------------
------------------------------------------------------------------------*/
void ResetEmitter(peDefPtr ped)
{
  peTexPtr pet = ped->peTex;
  int b;

  pet->emitting = NO_BANDS;

  for(b = 0; b < ped->outFlo.bands; ++b)
    ResetBand(&pet->emitter[b]);
}                               /* end ResetEmitter */


/*------------------------------------------------------------------------
------------------- get rid of left over strips etc. ---------------------
------------------------------------------------------------------------*/
void ResetBand(bandPtr bnd)
{
  bnd->replicate = NO_BANDS;

  FreeStrips(&bnd->stripLst);

  if(bnd->dataMap)
     bnd->dataMap = (CARD8 **)XieFree(bnd->dataMap);
}                               /* end ResetBand */


/*------------------------------------------------------------------------
-------------------------------- Link -----------------------------------
------------------------------------------------------------------------*/
static int flo_link(floDefPtr flo)
{
  peDefPtr ped;
  ddElemVecRec vec;
  pedLstPtr lst = ListEmpty(&flo->optDAG) ? &flo->defDAG : &flo->optDAG;
  
  /* create and initialize our photoflo's execution context */
  if(!flo->floTex && !(flo->floTex = (floTexPtr) XieMalloc(sizeof(floTexRec))))
    FloAllocError(flo,0,0, return(FALSE));

  flo->floTex->yieldPtr = NULL;

  /* create new element execution contexts for elements that have changed */
  for(ped = lst->flink; !ListEnd(ped,lst); ped = ped->flink)
    if(flo->flags.modified) {		/* XXX should be ped->flags.modified */
      if(ped->peTex) {			/*     fix this after beta release   */
	vec = ped->ddVec;		/*     shouldn't have to save vetors */
	Destroy(flo,ped);	/* destroy the old element context */
	ped->ddVec = vec;		/*     shouldn't have to restore them*/
      }
      if(!Create(flo,ped))
        return(FALSE);
    }
  return(TRUE);
}                               /* end flo_link */


/*------------------------------------------------------------------------
-------------------------------- Startup -----------------------------------
------------------------------------------------------------------------*/
static int flo_startup(floDefPtr flo)
{
  peDefPtr ped;
  pedLstPtr lst = ListEmpty(&flo->optDAG) ? &flo->defDAG : &flo->optDAG;
  int status;

  /* initialize our assistant managers */
  InitScheduler(flo);
  InitStripManager(flo);

  flo->floTex->imports = flo->floTex->exports = 0;
  flo->floTex->exitCnt = 0;
  
  /* initialize all the elements */
  for(ped = lst->flink; !ListEnd(ped,lst); ped = ped->flink) {
    ped->peTex->admissionCnt = 0;
    ped->peTex->schedCnt     = 0;
    ped->peTex->scheduled    = 0;
    if(Initialize(flo,ped))
      ped->flags.modified = FALSE;
    else
      break;
  }
  flo->flags.active   = TRUE;
  flo->flags.aborted  = FALSE;
  flo->flags.modified = FALSE;

  /* Call the scheduler -- there are no ImportClient elements the first time
   */
  if(ferrCode(flo)) {
    flo_shutdown(flo);
    status = FALSE;
  } else {
    status = Execute(flo, NULL);
  }
  return(status);
}                               /* end flo_startup */


/*------------------------------------------------------------------------
-------------------------------- Resume -----------------------------------
------------------------------------------------------------------------*/
static int flo_resume(floDefPtr flo)
{
  /* not implemented in the SI */
  
  FloImplementationError(flo,0,0, return(FALSE));
}                               /* end flo_resume */


/*------------------------------------------------------------------------
------------------------------ Shutdown ----------------------------------
------------------------------------------------------------------------*/
static int flo_shutdown(floDefPtr flo)
{
  peDefPtr ped;
  pedLstPtr lst = ListEmpty(&flo->optDAG) ? &flo->defDAG : &flo->optDAG;
  
  if(flo->floTex) {
    /* reset all the elements */
    for(ped = lst->flink; !ListEnd(ped,lst); ped = ped->flink)
      Reset(flo,ped);
    
    /* empty the strip cache */
    flo->floTex->stripSize = 0;
    if(flo->floTex->stripHead.flink)
      FreeStrips(&flo->floTex->stripHead);
    
    if(flo->awakenPtr) {
      /* awaken snoozing clients
       */
      while(flo->awakenCnt) {
	ClientPtr client = flo->awakenPtr[--flo->awakenCnt];
	if(!client->clientGone)
	  AttendClient(client);
      }
      flo->awakenPtr = (ClientPtr*)XieFree(flo->awakenPtr);
    }
    flo->flags.active = FALSE;
  }        
  return(TRUE);
}                               /* end flo_shutdown */


/*------------------------------------------------------------------------
-------------------------------- Destroy ---------------------------------
------------------------------------------------------------------------*/
static int flo_destroy(floDefPtr flo)
{
  peDefPtr ped;
  pedLstPtr lst = ListEmpty(&flo->optDAG) ? &flo->defDAG : &flo->optDAG;
  
  if(flo->floTex) {
    /* destroy all the lingering element structures */
    for(ped = lst->flink; !ListEnd(ped,lst); ped = ped->flink)
      Destroy(flo,ped);
    
    /* get rid of the floTex */
    flo->floTex = (floTexPtr) XieFree(flo->floTex);
  }
  /* zap the DDXIE photoflo management vectors */
  flo->floVec   = NULL;
  flo->schedVec = NULL;
  flo->stripVec = NULL;
  
  return(TRUE);
}                               /* end flo_destroy */

/* end module floman.c */
