/* $Xorg: sched.c,v 1.4 2001/02/09 02:04:23 xorgcvs Exp $ */
/**** module sched.c ****/
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
  
	sched.c -- DDXIE machine independent photoflo scheduler

	Dean Verheiden, Robert NC Shelley -- AGE Logic, Inc. May, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/control/sched.c,v 1.6 2001/12/14 19:58:17 dawes Exp $ */

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
#include <flostr.h>
#include <element.h>
#include <texstr.h>

#define SCHED_BAIL_OUT 10000 /* number of nonproductive times around scheduler
			      * loop before bailing out -- bad element
			      */

/*
 *  routines used internal to this module
 */
static Bool    execute(floDefPtr flo, peTexPtr importer);
static bandMsk runnable(floDefPtr flo, peTexPtr pet);

/*
 * DDXIE photoflo manager entry points
 */
static schedVecRec schedulerVec = {
  execute,
  runnable
  };


/*------------------------------------------------------------------------
--------------------------- Initialize Scheduler -------------------------
------------------------------------------------------------------------*/
int InitScheduler(floDefPtr flo)
{
  /* plug in the DDXIE scheduler vector */
  flo->schedVec = &schedulerVec;
  
  /* init the scheduler ready-list */
  ListInit(&flo->floTex->schedHead);
  
  return(TRUE);
}                               /* end InitScheduler */


/*------------------------------------------------------------------------
------------------------- Execute the photoflo ---------------------------
------------------------------------------------------------------------*/
static Bool execute(floDefPtr flo, peTexPtr importer)
{
  bandMsk ready;
  peTexPtr  pet;
  peDefPtr  ped;
  pedLstPtr lst = ListEmpty(&flo->optDAG) ? &flo->defDAG : &flo->optDAG;
  CARD32    sched_count = SCHED_BAIL_OUT;
  CARD32    strip_count = flo->floTex->putCnt;

  if(importer) {
    /* Put the ImportClient element at the head of the ready-list */
    InsertMember(importer,&flo->floTex->schedHead);
    importer->scheduled = importer->receptor[IMPORT].ready;
  }
  do {
    /* execute elements from the head of the ready-list until it's empty
     *    (calls to schedule from the data manager may prepend
     *     additional elements to the ready-list)
     */
    while(!ListEmpty(&flo->floTex->schedHead)) {
      pet = flo->floTex->schedHead.flink;
      
      if(Activate(flo,pet->peDef,pet) && (ready = runnable(flo,pet))) {
	pet->scheduled = ready;	/* remember which bands keep us alive */
      } else {
	/* element is no longer runnable, remove it and check for errors
	 */
	RemoveMember(pet,pet);
	pet->scheduled = 0;
	if(ferrCode(flo))
	  return(flo->flags.active = FALSE);
      }
      if(strip_count != flo->floTex->putCnt) {
	sched_count = SCHED_BAIL_OUT;
	strip_count = flo->floTex->putCnt;
      } else if( !--sched_count)
	ImplementationError(flo,pet->peDef, return(FALSE));
    }
    /* Load all the elements onto the ready-list that can keep producing
     * output without requiring any additional input (e.g. ImportResource
     * elements).
     */
    for(ped = lst->flink; !ListEnd(ped,lst); ped = ped->flink)
      if(ped->peTex->emitting && !ped->peTex->admissionCnt)
	InsertMember(ped->peTex,&flo->floTex->schedHead);
    /*
     *  keep on trucking if there's nothing expected from the client
     */
  } while(!flo->floTex->imports && !ListEmpty(&flo->floTex->schedHead));
  
  /* if we still have stuff to do, count another round, otherwise shut it down
   */
  if(flo->floTex->imports || flo->floTex->exports)
    ++flo->floTex->exitCnt;
  else
    ddShutdown(flo);

  return(flo->flags.active);
}                               /* end execute */


/*------------------------------------------------------------------------
----------------------- test element for runnability ---------------------
------------------------------------------------------------------------*/
static bandMsk runnable(floDefPtr flo, peTexPtr pet)
{
  receptorPtr rcp = pet->receptor, rend = &pet->receptor[pet->peDef->inCnt];
  bandMsk R, r;

  if(ferrCode(flo))
    return(0);

  if(pet->inSync)
    if(pet->bandSync) {
      /*
       * inSync true,  bandSync true: all attendable bands and inputs needed
       */
      for(R = ~0; rcp < rend && R; R &= r, ++rcp)
	if((rcp->active & rcp->attend) != (r = rcp->ready & rcp->attend))
	  return(NO_BANDS);
      return(R);			/* return AND of attendable bands */
    } else {
      /*
       * inSync true, bandSync false: any bands that match between all inputs
       */
      for(r = 0, R = ~0; rcp < rend; R &= r, ++rcp)
	if(rcp->active & rcp->attend && !(r = rcp->ready & rcp->attend))
	  return(NO_BANDS);
      return(R & r);			/* return AND of attendable bands */
    }
  else
    if(pet->bandSync) {
      /* 
       * inSync false, bandSync true: all bands of any input must be ready
       */
      for(R = 0; rcp < rend; R |= r, ++rcp)
	if((rcp->active & rcp->attend) != (r = rcp->ready & rcp->attend))
	  return(NO_BANDS);
      return(R);			/* return OR of attendable bands */
    } else {
      /*
       * inSync false, bandSync false: any band from any input will do
       */
      for(R = 0; rcp < rend; R |= rcp->ready & rcp->attend, ++rcp);
      return(R);			/* return OR of attendable bands */
    }  
}                               /* end runnable */

/* end module sched.c */
