/* $Xorg: flo.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/**** module flo.c ****/
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
  
	flo.c -- DIXIE photoflo utility routines
  
	Robert NC Shelley -- AGE Logic, Inc. May, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/request/flo.c,v 3.6 2001/12/14 19:58:09 dawes Exp $ */

/*
 *  Include files
 */

/*
 *  Core X Includes
 */
#define NEED_EVENTS
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
#include <corex.h>
#include <error.h>
#include <macro.h>
#include <flostr.h>
#include <tables.h>
/*
 * Module specific includes
 */
#include <memory.h>

/*
 *  routines used internal to this module
 */
static void   DAGonize(floDefPtr flo, peDefPtr ped);
static Bool   InputsOK(peDefPtr old, peDefPtr new);


/*------------------------------------------------------------------------
----------------------------- routine: MakeFlo ---------------------------
------------------------------------------------------------------------*/
floDefPtr MakeFlo(
     ClientPtr  client,
     CARD16     peCnt,
     xieFlo    *peLst)
{
  xieTypPhototag tag;
  xieFlo    *pe;
  peDefPtr  ped, export = NULL;
  floDefPtr flo; 
  
  if( !(flo = (floDefPtr) XieCalloc(sizeof(floDefRec) +
				    (peCnt+1) * sizeof(xieFlo *))) )
    return(NULL);
  
  flo->reqClient  =  client;
  flo->peCnt      =  peCnt;
  flo->peArray    = (peDefPtr *) &flo[1];
  flo->flags.modified = TRUE;
  ListInit(&flo->defDAG);
  ListInit(&flo->optDAG);
  
  /* allocate photo element definition structures for each client element
   */
  for(pe = peLst, tag = 1; tag <= peCnt && !ferrCode(flo); tag++) {
    if(client->swapped) {
      register int n;
      swaps(&pe->elemType, n);
      swaps(&pe->elemLength, n);
    }
    /* if it's a valid element, Make it -- then hop to the next one
     */
    if( pe->elemType <= xieMaxElem ) {
      flo->peArray[tag] = MakeElement(flo, tag, pe);
      pe = (xieFlo *)((CARD32 *)pe + pe->elemLength);
    } else
      FloElementError(flo,tag,pe->elemType, return(flo));
  }
  /* analyze the DAG's topology and connect it all together
   */
  for(tag = 1; tag <= peCnt && !ferrCode(flo); tag++) {
    ped = flo->peArray[tag];
    if(!ped->flags.export) continue;    /* skip to the next export element */
    ped->clink = export;                /* link it to previous exports     */
    export     = ped;
    /* link all elements together that contribute data towards this export */
    DAGonize(flo,ped);
  }
  return(flo);
}                               /* end MakeFlo */

			
/*------------------------------------------------------------------------
----------------------------- routine: EditFlo ---------------------------
------------------------------------------------------------------------*/
Bool EditFlo(
     floDefPtr      flo,
     xieTypPhototag start,
     xieTypPhototag end,
     xieFlo        *peLst)
{
  xieTypPhototag tag;
  pointer ptr;
  xieFlo   *pe;
  peDefPtr old, tmp;

  for(pe = peLst, tag = start; !ferrCode(flo) && tag <= end; tag++) {
    if(flo->reqClient->swapped) {
      register int n;
      swaps(&pe->elemType, n);
      swaps(&pe->elemLength, n);
    }
    old = flo->peArray[tag];
    if(pe->elemType != old->elemRaw->elemType)
      FloElementError(flo,tag,pe->elemType, return(FALSE));

    if( pe->elemType <= xieMaxElem ) {
      /* make a temporary peDef to hold the new client parameters    */
      if ((tmp = MakeElement(flo, tag, pe)) != 0) {
        if(InputsOK(old,tmp)) {
          /* swap the new parameter pointers into the existing peDef */
          SwapPtr(old->elemRaw,tmp->elemRaw,ptr);
          SwapPtr(old->elemPvt,tmp->elemPvt,ptr);
          SwapPtr(old->techPvt,tmp->techPvt,ptr);
          SwapPtr(old->techVec,tmp->techVec,ptr);
          old->flags.modified = TRUE;
	  /* free the tmp peDef which now holds the old parameters   */
	  FreePEDef(tmp);
        } else {
	  FreePEDef(tmp);
	  SourceError(flo,old, return(FALSE));
	}
      } else {
        return(FALSE);
      }
      pe = (xieFlo *)((CARD32 *)pe + pe->elemLength);
    } else
      FloElementError(flo,tag,pe->elemType, return(FALSE));
  }
  return(flo->flags.modified = TRUE);
}                               /* end EditFlo */

			
/*------------------------------------------------------------------------
----------------------------- routine: PrepFlo ---------------------------
------------------------------------------------------------------------*/
void PrepFlo(floDefPtr flo)
{
  peDefPtr  ped;
  pedLstPtr lst = ListEmpty(&flo->optDAG) ? &flo->defDAG : &flo->optDAG;
  
  for(ped = lst->flink; !ListEnd(ped,lst); ped = ped->flink)
    /*
     * call each element's Prep routine, which will:
     *	- lookup any resources needed by the element (e.g. Colormap, ROI, ...)
     *	- validate the element's parameter (and technique) values
     *	- validate the attributes of each source of data for the element
     *	- determine the element's output attributes
     *	- determine if anything was modified since a previous execution
     */
    if( !(*ped->diVec->prep)(flo, ped) )
      break;
}                               /* end PrepFlo */

			
/*------------------------------------------------------------------------
----------------------------- routine: FreeFlo ---------------------------
------------------------------------------------------------------------*/
floDefPtr FreeFlo(floDefPtr flo)
{
  peDefPtr ped;
  xieTypPhototag tag;
  
  while( !ListEmpty(&flo->optDAG) ) {
    /* free peDefs from the optimized DAG */
    RemoveMember(ped, flo->optDAG.flink);
    FreePEDef(ped);
  }
  for(tag = 1; tag <= flo->peCnt; tag++) {
    /* free the peDef and parameter block for each client element */
    FreePEDef(flo->peArray[tag]);
  }
  /* finally, free the floDef itself */
  XieFree(flo);  

  return(NULL);
}                               /* end FreeFlo */


/*------------------------------------------------------------------------
---- routine: Alloc a peDef and DIXIE parameter storage for an element ---
------------------------------------------------------------------------*/
peDefPtr MakePEDef(
     CARD32 inFloCnt,
     CARD32 rawLen,
     CARD32 pvtLen)
{
  int i, b;
  inFloPtr inf;
  peDefPtr ped = (peDefPtr) XieCalloc(sizeof(peDefRec) +
				      sizeof(inFloRec) * inFloCnt);
  if( ped ) {
    /* alloc some space for a copy of the client's element parameters */
    if( !(ped->elemRaw = (xieFlo *) XieMalloc(rawLen)))
      return(FreePEDef(ped));

    /* alloc whatever private space this element needs for dixie info */
    if(pvtLen)
      if ((ped->elemPvt = (pointer)XieCalloc(pvtLen)) != 0)
	*(CARD32 *)ped->elemPvt = pvtLen;
      else
	ped = FreePEDef(ped);
    ped->flags.modified = TRUE;

    /* init the outFlo */
    for(b = 0; b < xieValMaxBands; ++b) {
      ListInit(&ped->outFlo.output[b]);
      ped->outFlo.format[b].band = b;
    }

    /* init the in-line inFlo list */
    inf = (inFloPtr) &ped[1];
    ped->inFloLst = inf;
    ped->inCnt    = inFloCnt;
    for(i = 0; i < inFloCnt; (inf++)->index = i++)
      for(b = 0; b < xieValMaxBands; ++b)
	inf->format[b].band = b;
  }
  return(ped);
}                               /* end MakePEDef */


/*------------------------------------------------------------------------
---------------------------- routine: FreePEDef --------------------------
------------------------------------------------------------------------*/
peDefPtr FreePEDef(peDefPtr ped)
{
  int b;

  if( ped ) {
    /*
     * empty the outFlo
     */
    for(b = 0; b < xieValMaxBands; ++b) {
      if(!ListEmpty(&ped->outFlo.output[b]))
	 FreeStrips(&ped->outFlo.output[b]);
    }
    /* free element parameter structures
     */
    if( ped->elemRaw )	XieFree(ped->elemRaw);
    if( ped->elemPvt )	XieFree(ped->elemPvt);
    if( ped->techPvt )	XieFree(ped->techPvt);

    XieFree(ped);
  }
  return(NULL);
}                               /* end FreePEDef */


/*------------------------------------------------------------------------
----------------------------- Send Client Data ---------------------------
------------------------------------------------------------------------*/
void SendClientData(
  floDefPtr flo,
  peDefPtr  ped,
  CARD8   *data,
  CARD32  bytes,
  CARD8   swapUnits,
  CARD8   state)
{
  xieGetClientDataReply rep;
  
  if(flo->reqClient->clientGone) return;

  bzero((char *)&rep, sz_xieGetClientDataReply);
  rep.newState    = state;
  rep.type        = X_Reply;
  rep.sequenceNum = flo->reqClient->sequence;
  rep.length      = (bytes + 3) >> 2;
  rep.byteCount   = bytes;
  
  if( flo->reqClient->swapped ) {      
    register int n;
    swaps(&rep.sequenceNum, n);
    swapl(&rep.length, n);
    swapl(&rep.byteCount, n);
  }
  WriteToClient(flo->reqClient, sz_xieGetClientDataReply, (char *)&rep);
  
  if( bytes ) {
    /* if the data needs to be swapped, do it now
     */
    if( flo->reqClient->swapped ) switch(swapUnits) {
    case  0:
    case  1:
      break;
    case  2:
       SwapShorts((short*)data,bytes>>1);
      break;
    case  4:
    case  8:
    case 16:
       SwapLongs((CARD32*)data,bytes>>2);
      break;
    }
    WriteToClient(flo->reqClient, bytes, (char *)data);
  }
}                               /* end SendClientData */


/*------------------------------------------------------------------------
------- Based on levels, updata the passed in peDef's outFlo format-------
------------------------------------------------------------------------*/
Bool UpdateFormatfromLevels(peDefPtr ped)
{
  int i,bits;
  
  for(i = 0; i < ped->outFlo.bands; i++) {
    SetDepthFromLevels(ped->outFlo.format[i].levels, 
		       ped->outFlo.format[i].depth);
    
    if((bits = ped->outFlo.format[i].depth) > MAX_DEPTH(ped->outFlo.bands) ||
	ped->outFlo.format[i].levels < 2)
      return(FALSE);
    
    else if(bits == 1) {
      ped->outFlo.format[i].class  = BIT_PIXEL;
      ped->outFlo.format[i].stride = 1;
    } else if(bits <= 8) {
      ped->outFlo.format[i].class  = BYTE_PIXEL;
      ped->outFlo.format[i].stride = 8;
    } else if(bits <= 16) {
      ped->outFlo.format[i].class  = PAIR_PIXEL;
      ped->outFlo.format[i].stride = 16;
    } else {
      ped->outFlo.format[i].class  = QUAD_PIXEL;
      ped->outFlo.format[i].stride = 32;
    }
    bits = ped->outFlo.format[i].width * ped->outFlo.format[i].stride;
    ped->outFlo.format[i].pitch = bits + Align(bits,PITCH_MOD);
  }
  return (TRUE);
}


/*------------------------------------------------------------------------*
 * DAGonize recurses back through all the elements that contribute data   *
 * to the element passed to it.  The recursion terminates when:           *
 *   - an import element is found                                         *
 *   - an element with fully connected inputs is found                    *
 *   - an error is encountered (phototag out of range, or loop detected)  *
 *------------------------------------------------------------------------*/
static void DAGonize(floDefPtr flo, peDefPtr ped)
{
  int  in, tag;
  peDefPtr src;
  inFloPtr inFlo;
  
  if(ped->flink) return;	/* this element has already been DAGonized  */

  /* connect all the inputs to this element */
  for(ped->flags.loop = TRUE,in = 0; in < ped->inCnt && !ferrCode(flo); in++) {
    inFlo = &ped->inFloLst[in];
    if((tag = inFlo->srcTag) > flo->peCnt)
      SourceError(flo,ped, break);	/* input is outside the flo-graph!  */
    
    if(tag) {		  /* only connect specified (i.e. non-zero) inputs  */
      inFlo->ownDef = ped;
      src = inFlo->srcDef = flo->peArray[tag];
      
      if(src->flags.loop || src->flags.export)
	SourceError(flo,ped, break);	/* oops, we've stumbled over a loop */

      /* insert this inFlo into the source element's outFlo list */
      inFlo->outChain = src->outFlo.outChain;
      src->outFlo.outChain = inFlo;

      /* connect all the elements that contribute data towards this input   */
      DAGonize(flo,src);
    }        
  }
  if(!ferrCode(flo)) {
    /* clear the loop-detector */
    ped->flags.loop = FALSE;
    
    if(ped->flags.import && !ListEmpty(&flo->defDAG)) {
      /* find the end of the import list, then append this element */
      for(src = flo->defDAG.flink; src->clink; src = src->clink);
      src->clink = ped;
    }
    /* finally -- append this element onto the definition DAG */
    InsertMember(ped, flo->defDAG.blink);
  }
}                               /* end DAGonize */


/*------------------------------------------------------------------------
-------------- compare input connections between two peDefs --------------
------------------------------------------------------------------------*/
static Bool InputsOK(peDefPtr old, peDefPtr new)
{
  inFloPtr oldin = old->inFloLst, newin = old->inFloLst;
  int i;

  if(old->inCnt != new->inCnt)
    return(FALSE);

  for(i = 0; i < old->inCnt; oldin++, newin++, i++)
    if(oldin->srcTag != newin->srcTag)
      return(FALSE);

  return(TRUE);
}

/* end module flo.c */
