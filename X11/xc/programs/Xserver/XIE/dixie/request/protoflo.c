/* $Xorg: protoflo.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module protoflo.c ****/
/****************************************************************************

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

	protoflo.c: photospace and photoflo request/reply procedures

	Robert NC Shelley, Dean Verheiden -- AGE Logic, Inc., May 1993

****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/request/protoflo.c,v 3.5 2001/12/14 19:58:10 dawes Exp $ */


#include <protoflo.h>

#include <corex.h>
#include <macro.h>
#include <memory.h>

/*
 *  routines used internal to this module
 */
static floDefPtr LookupExecutable(CARD32 spaceID, CARD32 floID);
static floDefPtr LookupImmediate(CARD32  spaceID, CARD32  floID, photospacePtr *spacePtr);
static int       RunFlo(ClientPtr client, floDefPtr flo);
static int       FloDone(floDefPtr flo);
static void      DeleteImmediate(floDefPtr flo);



/*------------------------------------------------------------------------
--------------------------- CreatePhotospace Procedure -------------------
------------------------------------------------------------------------*/
int ProcCreatePhotospace(ClientPtr client)
{
  photospacePtr space;
  REQUEST(xieCreatePhotospaceReq);
  REQUEST_SIZE_MATCH(xieCreatePhotospaceReq);
  LEGAL_NEW_RESOURCE(stuff->nameSpace, client);

  /*
   * create a new lookup table
   */
  if(!(space = (photospacePtr) XieMalloc(sizeof(photospaceRec)))) 
    return(client->errorValue = stuff->nameSpace, BadAlloc);
  
  space->spaceID = stuff->nameSpace;
  space->floCnt  = 0;
  ListInit(&space->floLst);

  return( AddResource(space->spaceID, RT_PHOTOSPACE, space)
	? Success : (client->errorValue = stuff->nameSpace, BadAlloc) );
}                               /* end ProcCreatePhotospace */


/*------------------------------------------------------------------------
------------------------ DestroyPhotospace Procedure ---------------------
------------------------------------------------------------------------*/
int ProcDestroyPhotospace(ClientPtr client)
{
  photospacePtr space;
  REQUEST( xieDestroyPhotospaceReq );
  REQUEST_SIZE_MATCH( xieDestroyPhotospaceReq );
  
  if(!(space = (photospacePtr)LookupIDByType(stuff->nameSpace,RT_PHOTOSPACE)))
    return( SendResourceError(client, xieErrNoPhotospace, stuff->nameSpace) );
  
  /*
   * Disassociate the Photospace from core X -- it calls DeletePhotospace()
   */
  FreeResourceByType(stuff->nameSpace, RT_PHOTOSPACE, RT_NONE);
  
  return(Success);
}                               /* end ProcDestroyPhotospace */


/*------------------------------------------------------------------------
------------------------ ExecuteImmediate Procedure ----------------------
------------------------------------------------------------------------*/
int ProcExecuteImmediate(ClientPtr client)
{
  floDefPtr flo;
  photospacePtr space;
  REQUEST(xieExecuteImmediateReq);
  REQUEST_AT_LEAST_SIZE(xieExecuteImmediateReq);

  /* verify that the new flo-id is unique */
  flo = LookupImmediate(stuff->nameSpace, stuff->floID, &space);
  if( !space || flo )
    return(SendFloIDError(client,stuff->nameSpace,stuff->floID));
  
  /* create the flo structures and verify the DAG's topology */
  if(!(flo = MakeFlo(client, stuff->numElements, (xieFlo *)&stuff[1])))
    return(client->errorValue = stuff->floID, BadAlloc);
  
  /* append the new flo to the photospace */
  flo->space        = space;
  flo->spaceID      = stuff->nameSpace;
  flo->ID           = stuff->floID;
  flo->flags.notify = stuff->notify;
  space->floCnt++;
  InsertMember(flo,space->floLst.blink);

  /* try to execute it */
  return(RunFlo(client,flo));
}                               /* end ProcExecuteImmediate */


/*------------------------------------------------------------------------
------------------------- CreatePhotoflo Procedure -----------------------
------------------------------------------------------------------------*/
int ProcCreatePhotoflo(ClientPtr client)
{
  floDefPtr flo;
  REQUEST(xieCreatePhotofloReq);
  REQUEST_AT_LEAST_SIZE(xieCreatePhotofloReq);
  LEGAL_NEW_RESOURCE(stuff->floID, client);

  /* create a new Photoflo
   */
  if( !(flo = MakeFlo(client, stuff->numElements, (xieFlo *)&stuff[1])) )
    return(client->errorValue = stuff->floID, BadAlloc);
  flo->ID = stuff->floID;
  
  if( ferrCode(flo) ) {
    SendFloError(client,flo);
    DeletePhotoflo(flo, stuff->floID);
    return(Success);
  }
  /* All is well, try to register the new flo
   */
  return( AddResource(stuff->floID, RT_PHOTOFLO, (floDefPtr)flo)
	 ? Success : (client->errorValue = stuff->floID, BadAlloc) );
}                               /* end ProcCreatePhotoflo */


/*------------------------------------------------------------------------
------------------------ DestroyPhotoflo Procedure -----------------------
------------------------------------------------------------------------*/
int ProcDestroyPhotoflo(ClientPtr client)
{
  floDefPtr flo;
  REQUEST( xieDestroyPhotofloReq );
  REQUEST_SIZE_MATCH( xieDestroyPhotofloReq );
  
  if( !(flo = (floDefPtr) LookupIDByType(stuff->floID, RT_PHOTOFLO)) )
    return( SendResourceError(client, xieErrNoPhotoflo, stuff->floID) );
  
  /* Disassociate the Photoflo from core X -- it calls DeletePhotoflo()
   */
  FreeResourceByType(stuff->floID, RT_PHOTOFLO, RT_NONE);
  
  return(Success);
}                               /* end ProcDestroyPhotoflo */


/*------------------------------------------------------------------------
------------------------ ExecutePhotoflo Procedure -----------------------
------------------------------------------------------------------------*/
int ProcExecutePhotoflo(ClientPtr client)
{
  floDefPtr flo;
  REQUEST( xieExecutePhotofloReq );
  REQUEST_SIZE_MATCH( xieExecutePhotofloReq );
  
  if( !(flo = (floDefPtr) LookupIDByType(stuff->floID, RT_PHOTOFLO)) )
    return( SendResourceError(client, xieErrNoPhotoflo, stuff->floID) );
  
  if( flo->flags.active ) 
    FloAccessError(flo,0,0, return(SendFloError(client,flo)));
  flo->flags.notify = stuff->notify;
  ferrCode(flo) = 0;

  /* try to execute it */
  return(RunFlo(client,flo));
}                               /* end ProcExecutePhotoflo */


/*------------------------------------------------------------------------
------------------------- ModifyPhotoflo Procedure -----------------------
------------------------------------------------------------------------*/
int ProcModifyPhotoflo(ClientPtr client)
{
  floDefPtr flo;
  xieTypPhototag end;
  REQUEST( xieModifyPhotofloReq );
  REQUEST_AT_LEAST_SIZE(xieModifyPhotofloReq);

  if( !(flo = (floDefPtr) LookupIDByType(stuff->floID, RT_PHOTOFLO)) )
    return( SendResourceError(client, xieErrNoPhotoflo, stuff->floID) );
  
  if( flo->flags.active )
    FloAccessError(flo,0,0, goto egress);

  if(!stuff->start || stuff->start > flo->peCnt)
    FloSourceError(flo,stuff->start,0, goto egress);
  
  if((end = stuff->start + stuff->numElements - 1) > flo->peCnt)
    FloElementError(flo,flo->peCnt,0, goto egress);
  
  /* edit existing elements according to the list of elements we were given
   */
  EditFlo(flo, stuff->start, end, (xieFlo *)&stuff[1]);

 egress:
  return(ferrCode(flo) ? SendFloError(client,flo) : Success);
}                               /* end ProcModifyPhotoflo */


/*------------------------------------------------------------------------
------------------------ RedefinePhotoflo Procedure ----------------------
------------------------------------------------------------------------*/
int ProcRedefinePhotoflo(ClientPtr client)
{
  floDefPtr old, new;
  REQUEST( xieRedefinePhotofloReq );
  REQUEST_AT_LEAST_SIZE(xieRedefinePhotofloReq);
  
  if( !(old = (floDefPtr) LookupIDByType(stuff->floID, RT_PHOTOFLO)) )
    return( SendResourceError(client, xieErrNoPhotoflo, stuff->floID) );
  
  if( old->flags.active ) 
    FloAccessError(old,0,0, return(SendFloError(client,old)));
  
  /* create a new Photoflo
   */
  if( !(new = MakeFlo(client, stuff->numElements, (xieFlo *)&stuff[1])) )
    return(client->errorValue = stuff->floID, BadAlloc);
  new->ID = stuff->floID;
  
  if( ferrCode(new) ) {
    SendFloError(client,new);
    DeletePhotoflo(new, stuff->floID);
    return(Success);
  }
  /* Disassociate the old flo from core X -- it calls DeletePhotoflo()
   */
  FreeResourceByType(stuff->floID, RT_PHOTOFLO, RT_NONE);
  
  /* Then (re)register the new flo using the old flo's ID
   */
  return( AddResource(stuff->floID, RT_PHOTOFLO, (floDefPtr)new)
	 ? Success : (client->errorValue = stuff->floID, BadAlloc) );
}                               /* end ProcRedefinePhotoflo */


/*------------------------------------------------------------------------
------------------------------ Abort Procedure ---------------------------
------------------------------------------------------------------------*/
int ProcAbort(ClientPtr client)
{
  floDefPtr flo;
  REQUEST( xieAbortReq );
  REQUEST_SIZE_MATCH( xieAbortReq );
  
  if ((flo = LookupExecutable(stuff->nameSpace, stuff->floID)) != 0)
    if( flo->flags.active ) {
      flo->reqClient     = client;
      flo->flags.aborted = TRUE;
      ddShutdown(flo);
      FloDone(flo);
    }
  return(Success);
}                               /* end ProcAbort */


/*------------------------------------------------------------------------
------------------------------ Await Procedure ---------------------------
------------------------------------------------------------------------*/
int ProcAwait(ClientPtr client)
{
  ClientPtr *awaken;
  floDefPtr flo;
  REQUEST( xieAwaitReq );
  REQUEST_SIZE_MATCH( xieAwaitReq );
  
  if( (flo = LookupExecutable(stuff->nameSpace, stuff->floID))
     && flo->flags.active ) {
    if((awaken = (ClientPtr*)(flo->awakenCnt
			     ? XieRealloc( flo->awakenPtr,
					  (flo->awakenCnt+1)*sizeof(ClientPtr))
			     : XieMalloc(sizeof(ClientPtr)))) != 0) {
      /*
       * tell core X to ignore this client until the flo is done
       */
      awaken[flo->awakenCnt++] = client;
      flo->awakenPtr = awaken;
      IgnoreClient(client);
    } else {
      return(BadAlloc);
    }
  }
  return(Success);
}                               /* end ProcAwait */


/*------------------------------------------------------------------------
-------------------------- Get Client Data Procedure ---------------------
------------------------------------------------------------------------*/
int ProcGetClientData(ClientPtr client)
{
  floDefPtr flo;
  peDefPtr  ped;
  REQUEST( xieGetClientDataReq );
  REQUEST_SIZE_MATCH( xieGetClientDataReq );
  
  /* find the flo and make sure it's active
   */
  if(!(flo = LookupExecutable(stuff->nameSpace, stuff->floID)))
    return SendFloIDError(client, stuff->nameSpace, stuff->floID);
  if(!flo->flags.active)
    FloAccessError(flo,stuff->element,0, return(SendFloError(client,flo)));
  
  /* verify that the specified element and band are OK
   */
  flo->reqClient = client;
  ped = (stuff->element && stuff->element <= flo->peCnt
	 ? flo->peArray[stuff->element] : NULL);
  if(!ped || !ped->flags.getData)
    FloElementError(flo, stuff->element, ped ? ped->elemRaw->elemType : 0,
                    goto egress);
  if(stuff->bandNumber >= ped->inFloLst[0].bands)
    ValueError(flo,ped,stuff->bandNumber, goto egress);
  
  /* grab some data and have it sent to the client
   */
  ddOutput(flo, ped, stuff->bandNumber, stuff->maxBytes, stuff->terminate);

 egress:  
  return(ferrCode(flo) || !flo->flags.active ? FloDone(flo) : Success);
}                               /* end ProcGetClientData */


/*------------------------------------------------------------------------
-------------------------- Put Client Data Procedure ---------------------
------------------------------------------------------------------------*/
int ProcPutClientData(ClientPtr client)
{
  floDefPtr flo;
  peDefPtr  ped;
  REQUEST( xiePutClientDataReq );
  REQUEST_AT_LEAST_SIZE(xiePutClientDataReq);
  
  /* find the flo and make sure it's active
   */
  if( !(flo = LookupExecutable(stuff->nameSpace, stuff->floID)) )
    return( SendFloIDError(client, stuff->nameSpace, stuff->floID) );
  if( !flo->flags.active )
    FloAccessError(flo,stuff->element,0, return(SendFloError(client,flo)));
  
  /* verify that the target element and band are OK
   */
  flo->reqClient = client;
  ped = stuff->element && stuff->element <= flo->peCnt
    ? flo->peArray[stuff->element] : NULL;
  if( !ped || !ped->flags.putData )
    FloElementError(flo, stuff->element, ped ? ped->elemRaw->elemType : 0,
                    goto egress);
  if( stuff->bandNumber >= ped->inFloLst[0].bands )
    ValueError(flo,ped,stuff->bandNumber, goto egress);
  
  /* check for partial aggregates and swap the data as required
   */
  switch(ped->swapUnits[stuff->bandNumber]) {
  case  0:
  case  1:
    break;
  case  2:
    if(stuff->byteCount & 1)
      ValueError(flo,ped,stuff->byteCount, goto egress);
    if (client->swapped) 
      SwapShorts((short*)&stuff[1],stuff->byteCount>>1);
    break;
  case  4:
  case  8:
  case 16:
    if(stuff->byteCount & (ped->swapUnits[stuff->bandNumber]-1))
      ValueError(flo,ped,stuff->byteCount, goto egress);
    if(client->swapped) 
      SwapLongs((CARD32*)&stuff[1],stuff->byteCount>>2);
    break;
  }
  /* pass the byte-stream to the target element
   */
  if(stuff->byteCount || stuff->final)
    ddInput(flo, ped, stuff->bandNumber,
	    (CARD8*)&stuff[1], stuff->byteCount, stuff->final);

 egress:
  return(ferrCode(flo) || !flo->flags.active ? FloDone(flo) : Success);
}                               /* end ProcPutClientData */


/*------------------------------------------------------------------------
------------------------ QueryPhotoflo Procedure -------------------------
------------------------------------------------------------------------*/
int ProcQueryPhotoflo(ClientPtr client)
{
  CARD16 imCnt, exCnt;
  CARD32 shorts;
  floDefPtr flo;
  xieTypPhototag *list;
  xieQueryPhotofloReply rep;
  REQUEST( xieQueryPhotofloReq );
  REQUEST_SIZE_MATCH( xieQueryPhotofloReq );
  
  bzero((char *)&rep, sz_xieQueryPhotofloReply);
  rep.state = ((flo = LookupExecutable(stuff->nameSpace, stuff->floID))
	       ? (flo->flags.active ? xieValActive : xieValInactive)
	       : xieValNonexistent);
  
  /* Ask ddxie about the status of client transport
   */
  if(!flo || !flo->flags.active)
    imCnt = exCnt = 0;
  else if(!ddQuery(flo,&list,&imCnt,&exCnt))
    return(SendFloError(client,flo));
  
  /* Fill in the reply header
   */
  shorts = ((imCnt + 1) & ~1) + ((exCnt + 1) & ~1);
  rep.type           = X_Reply;
  rep.sequenceNum    = client->sequence;
  rep.length         = shorts >> 1;
  rep.expectedCount  = imCnt;
  rep.availableCount = exCnt;
  
  if( client->swapped ) {      
    register int n;
    swaps(&rep.sequenceNum,n);
    swapl(&rep.length,n);
    swaps(&rep.expectedCount,n);
    swaps(&rep.availableCount,n);
  }
  WriteToClient(client, sz_xieQueryPhotofloReply, (char *)&rep);
  
  if(shorts) {
    /* Send the list of pending import/export(s) (swapped as necessary)
     */
    if( client->swapped )
      SwapShorts((short *)list, shorts);
    WriteToClient(client, shorts<<1, (char *)list);
    XieFree(list);
  }
  return(Success);
}                               /* end ProcQueryPhotoflo */


/*------------------------------------------------------------------------
----------------------- deleteFunc: DeletePhotospace ---------------------
------------------------------------------------------------------------*/
int DeletePhotospace(
     photospacePtr space,
     xieTypPhotospace id)
{
  /* abort and destroy all flos in the photospace
   */
  while( space->floCnt ) {
    floDefPtr flo = space->floLst.flink;
    /*
     * abort it's execution, and then let it go away quietly (no error/events)
     */
    flo->reqClient = flo->runClient;
    flo->flags.aborted = TRUE;
    flo->flags.notify  = FALSE;
    ddShutdown(flo);
    ferrCode(flo) = 0;
    FloDone(flo);
  }
  /* Free the Photospace structure.
   */
  XieFree(space);
  
  return(Success);
}                               /* end DeletePhotospace */


/*------------------------------------------------------------------------
----------------------- deleteFunc: DeletePhotoflo -----------------------
------------------------------------------------------------------------*/
int DeletePhotoflo(
     floDefPtr     flo,
     xieTypPhotoflo id)
{
  if(flo->flags.active) {
    /*
     * abort it's execution, and then let it go away quietly (no error/events)
     */
    flo->reqClient = flo->runClient;
    flo->flags.aborted = TRUE;
    flo->flags.notify  = FALSE;
    ddShutdown(flo);
    ferrCode(flo) = 0;
    FloDone(flo);
  }
  /* destroy any lingering ddxie structures
   */
  ddDestroy(flo);
  
  /* free the dixie element structures
   */
  FreeFlo(flo);
  
  return(Success);
}                               /* end DeletePhotoflo */


/*------------------------------------------------------------------------
------------------------- routine: LookupExecutable ----------------------
------------------------------------------------------------------------*/
static floDefPtr LookupExecutable(CARD32 spaceID, CARD32 floID)
{
  floDefPtr flo;
  
  if( spaceID )
    flo = LookupImmediate(spaceID, floID, NULL);
  else
    flo = (floDefPtr) LookupIDByType(floID, RT_PHOTOFLO);
  
  return(flo);
}                               /* end LookupExecutable */


/*------------------------------------------------------------------------
------------------------ routine: LookupImmediate ------------------------
------------------------------------------------------------------------*/
static floDefPtr LookupImmediate(
     CARD32  spaceID,
     CARD32  floID,
     photospacePtr *spacePtr)
{
  floDefPtr flo;
  photospacePtr space = (photospacePtr) LookupIDByType(spaceID, RT_PHOTOSPACE);
  
  if(spacePtr)
    *spacePtr = space;
  if(!space)
    return(NULL);
  
  /* search the photospace for the specified flo
   */
  for(flo = space->floLst.flink;
      !ListEnd(flo,&space->floLst) && floID != flo->ID;
      flo = flo->flink);
  
  return( ListEnd(flo,&space->floLst) ? NULL : flo );
}                               /* end LookupImmediate */


/*------------------------------------------------------------------------
----------- initiate, and possibly complete, photoflo execution ----------
------------------------------------------------------------------------*/
static int RunFlo(ClientPtr client, floDefPtr flo)
{
  flo->runClient = flo->reqClient = client;

  /* validate parameters and propagate attributes between elements */
  if( !ferrCode(flo) )
    PrepFlo(flo);
  
  /* choose the "best" set of handlers for this DAG (this also
   * establishes all DDXIE entry points in the floDef and peDefs)
   */
  if(!ferrCode(flo) && flo->flags.modified)
    DAGalyze(flo);
  
  /* create all the new handlers that were chosen by DAGalyze */
  if(!ferrCode(flo) && flo->flags.modified)
    ddLink(flo);
  
  /* begin (and maybe complete) execution */
  if( ferrCode(flo) || !ddStartup(flo) )
    FloDone(flo);
  
  return(Success);
}                               /* end RunFlo */


/*------------------------------------------------------------------------
-------- Handle Photoflo Done: send error and event, then clean up -------
------------------------------------------------------------------------*/
static int FloDone(floDefPtr flo)
{
  peDefPtr ped;
  pedLstPtr lst = ListEmpty(&flo->optDAG) ? &flo->defDAG : &flo->optDAG;
  Bool       ok = !ferrCode(flo) && !flo->flags.aborted;

  /* debrief import elements */
  for(ped = lst->flink; ped && !ListEnd(ped,lst); ped = ped->clink)
    if(ped->diVec->debrief)
      ok &= (*ped->diVec->debrief)(flo,ped,ok);

  /* debrief all other elements (e.g. export and ConvertToIndex) */
  for(ped = lst->flink; ped && !ListEnd(ped,lst); ped = ped->flink)
    if(!ped->flags.import && ped->diVec->debrief)
      ok &= (*ped->diVec->debrief)(flo,ped,ok);

  /* handle errors */
  if(ferrCode(flo)) {
    ddShutdown(flo);
    SendFloError(flo->runClient,flo);
    if(flo->reqClient != flo->runClient)
      SendFloError(flo->reqClient,flo);
  }
  /* handle events */
  if(flo->flags.notify) {
    flo->event.event = xieEvnNoPhotofloDone;
    
    if(ferrCode(flo))
      ((xiePhotofloDoneEvn *)&flo->event)->outcome = xieValFloError;
    else if(flo->flags.aborted)
      ((xiePhotofloDoneEvn *)&flo->event)->outcome = xieValFloAbort;
    else
      ((xiePhotofloDoneEvn *)&flo->event)->outcome = xieValFloSuccess;
    
    SendFloEvent(flo);
  }
  /* if this was an immediate flo, it's history */
  if(flo->spaceID)
    DeleteImmediate(flo);

  return(Success);
}                               /* end FloDone */


/*------------------------------------------------------------------------
------------------------- routine: DeleteImmediate -----------------------
------------------------------------------------------------------------*/
static void DeleteImmediate(floDefPtr flo)
{
  floDefPtr tmp;

  /* destroy any lingering DDXIE structures
   */
  ddDestroy(flo);

  /* remove the photoflo from the photospace and destroy it
   */
  flo->space->floCnt--;
  RemoveMember(tmp, flo);
  FreeFlo(tmp);
}                               /* end DeleteImmediate */


/*------------------------------------------------------------------------
----------------------------- Swap procedures ----------------------------
------------------------------------------------------------------------*/
int SProcCreatePhotospace(ClientPtr client)
{
  register long n;
  REQUEST(xieCreatePhotospaceReq);
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH(xieCreatePhotospaceReq);
  swapl(&stuff->nameSpace, n);
  return (ProcCreatePhotospace(client));
}                               /* end SProcCreatePhotospace */

int SProcDestroyPhotospace(ClientPtr client)
{
  register long n;
  REQUEST( xieDestroyPhotospaceReq );
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH( xieDestroyPhotospaceReq );
  swapl(&stuff->nameSpace, n);
  return (ProcDestroyPhotospace(client));
}                               /* end SProcDestroyPhotospace */

int SProcExecuteImmediate(ClientPtr client)
{
  register int n;
  REQUEST(xieExecuteImmediateReq);
  swaps(&stuff->length, n);
  REQUEST_AT_LEAST_SIZE(xieExecuteImmediateReq);
  swapl(&stuff->nameSpace, n);
  swapl(&stuff->floID, n);
  swaps(&stuff->numElements, n);
  return( ProcExecuteImmediate(client) );
}

int SProcCreatePhotoflo(ClientPtr client)
{
  register int n;
  REQUEST(xieCreatePhotofloReq);
  swaps(&stuff->length, n);
  REQUEST_AT_LEAST_SIZE(xieCreatePhotofloReq);
  swapl(&stuff->floID, n);
  swaps(&stuff->numElements, n);
  return( ProcCreatePhotoflo(client) );
}

int SProcDestroyPhotoflo(ClientPtr client)
{
  register int n;
  REQUEST(xieDestroyPhotofloReq);
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH(xieDestroyPhotofloReq);
  swapl(&stuff->floID, n);
  return( ProcDestroyPhotoflo(client) );
}

int SProcExecutePhotoflo(ClientPtr client)
{
  register int n;
  REQUEST(xieExecutePhotofloReq);
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH(xieExecutePhotofloReq);
  swapl(&stuff->floID, n);
  return( ProcExecutePhotoflo(client) );
}

int SProcModifyPhotoflo(ClientPtr client)
{
  register int n;
  REQUEST(xieModifyPhotofloReq);
  swaps(&stuff->length, n);
  REQUEST_AT_LEAST_SIZE(xieModifyPhotofloReq);
  swapl(&stuff->floID, n);
  swaps(&stuff->start, n);
  swaps(&stuff->numElements, n);
  return( ProcModifyPhotoflo(client) );
}

int SProcRedefinePhotoflo(ClientPtr client)
{
  register int n;
  REQUEST(xieRedefinePhotofloReq);
  swaps(&stuff->length, n);
  REQUEST_AT_LEAST_SIZE(xieRedefinePhotofloReq);
  swapl(&stuff->floID, n);
  swaps(&stuff->numElements, n);
  return( ProcRedefinePhotoflo(client) );
}

int SProcAbort(ClientPtr client)
{
  register int n;
  REQUEST(xieAbortReq);
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH(xieAbortReq);
  swapl(&stuff->nameSpace, n);
  swapl(&stuff->floID, n);
  return( ProcAbort(client) );
}

int SProcAwait(ClientPtr client)
{
  register int n;
  REQUEST(xieAwaitReq);
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH(xieAwaitReq);
  swapl(&stuff->nameSpace, n);
  swapl(&stuff->floID, n);
  return( ProcAwait(client) );
}

int SProcGetClientData(ClientPtr client)
{
  register int n;
  REQUEST(xieGetClientDataReq);
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH(xieGetClientDataReq);
  swapl(&stuff->nameSpace, n);
  swapl(&stuff->floID, n);
  swapl(&stuff->maxBytes, n);
  swaps(&stuff->element, n);
  return( ProcGetClientData(client) );
}

int SProcPutClientData(ClientPtr client)
{
  register int n;
  REQUEST(xiePutClientDataReq);
  swaps(&stuff->length, n);
  REQUEST_AT_LEAST_SIZE(xiePutClientDataReq);
  swapl(&stuff->nameSpace, n);
  swapl(&stuff->floID, n);
  swaps(&stuff->element, n);
  swapl(&stuff->byteCount, n);
  return( ProcPutClientData(client) );
}

int SProcQueryPhotoflo(ClientPtr client)
{
  register int n;
  REQUEST(xieQueryPhotofloReq);
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH(xieQueryPhotofloReq);
  swapl(&stuff->nameSpace, n);
  swapl(&stuff->floID, n);
  return( ProcQueryPhotoflo(client) );
}

/* end module protoflo.c */
