/* $Xorg: colorlst.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/**** module colorlst.c ****/
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
******************************************************************************
  
	colorlst.c -- DIXIE ColorList management
  
	Robert NC Shelley -- AGE Logic, Inc. March, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/request/colorlst.c,v 3.5 2001/12/14 19:58:09 dawes Exp $ */

#define _XIEC_COLORLST

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
#include <extnsionst.h>
#include <dixstruct.h>
#include <colormapst.h>
/*
 *  Module Specific Includes
 */
#include <corex.h>
#include <macro.h>
#include <memory.h>
#include <colorlst.h>
#include <tables.h>

/*
 *  routines referenced by other modules.
 */
int  DeleteColorList();
colorListPtr  LookupColorList();
void ResetColorList();


/*------------------------------------------------------------------------
------------------------ CreateColorList Procedures ----------------------
------------------------------------------------------------------------*/
int ProcCreateColorList(ClientPtr client)
{
  colorListPtr clst;
  REQUEST(xieCreateColorListReq);
  REQUEST_SIZE_MATCH(xieCreateColorListReq);
  LEGAL_NEW_RESOURCE(stuff->colorList, client);

  /*
   * create a new ColorList
   */
  if( !(clst = (colorListPtr) XieMalloc(sizeof(colorListRec))) )
    return(client->errorValue = stuff->colorList, BadAlloc);
  
  clst->ID      = stuff->colorList;
  clst->refCnt  = 1;
  clst->cellPtr = NULL;

  ResetColorList(clst, NULL);

  return( AddResource(clst->ID, RT_COLORLIST, (colorListPtr)clst)
	? Success : (client->errorValue = stuff->colorList, BadAlloc) );
}                               /* end ProcCreateColorList */


/*------------------------------------------------------------------------
------------------------ DestroyColorList Procedures ---------------------
------------------------------------------------------------------------*/
int ProcDestroyColorList(ClientPtr client)
{
  colorListPtr clst;
  REQUEST( xieDestroyColorListReq );
  REQUEST_SIZE_MATCH( xieDestroyColorListReq );
  
  if( !(clst = LookupColorList(stuff->colorList)) )
    return( SendResourceError(client, xieErrNoColorList, stuff->colorList) );
  
  /*
   * Disassociate the ColorList from core X -- it calls DeleteColorList()
   */
  FreeResourceByType(stuff->colorList, RT_COLORLIST, RT_NONE);
  
  return(Success);
}                               /* end ProcDestroyColorList */


/*------------------------------------------------------------------------
-------------------------- PurgeColorList Procedures ---------------------
------------------------------------------------------------------------*/
int ProcPurgeColorList(ClientPtr client)
{
  colorListPtr clst;
  REQUEST( xiePurgeColorListReq );
  REQUEST_SIZE_MATCH( xiePurgeColorListReq );
  
  if( !(clst = LookupColorList(stuff->colorList)) )
    return( SendResourceError(client, xieErrNoColorList, stuff->colorList) );

  /*
   * Free the current list of colors
   */
  ResetColorList(clst, clst->mapPtr);
  
  return(Success);
}                               /* end ProcPurgeColorList */


/*------------------------------------------------------------------------
------------------------ QueryColorList Procedures -----------------------
------------------------------------------------------------------------*/
int ProcQueryColorList(ClientPtr client)
{
  xieQueryColorListReply rep;
  colorListPtr clst;
  REQUEST( xieQueryColorListReq );
  REQUEST_SIZE_MATCH( xieQueryColorListReq );
  
  if( !(clst = LookupColorList(stuff->colorList)) )
    return( SendResourceError(client, xieErrNoColorList, stuff->colorList) );

  /*
   * Fill in the reply header
   */
  bzero((char *)&rep, sz_xieQueryColorListReply);
  rep.type        = X_Reply;
  rep.sequenceNum = client->sequence;
  rep.colormap    = clst->mapID;
  rep.length      = clst->cellCnt;
  
  if( client->swapped ) {      
    /*
     * Swap the reply header fields
     */
    register int n;
    swaps(&rep.sequenceNum,n);
    swapl(&rep.colormap,n);
    swapl(&rep.length,n);
  }
  WriteToClient(client, sz_xieQueryColorListReply, (char *)&rep);
  
  if( clst->cellCnt )
    /*
     * Send the list of colors (swapped as necessary)
     * Note: cellPtr is type Pixel, which unfortunately is type unsigned long
     *       and that means more work needed here if longs are 64-bits...
     *       (anyone care to donate an Alpha?)
     */
    if( client->swapped )
      CopySwap32Write(client, clst->cellCnt << 2, clst->cellPtr);
    else
      WriteToClient(client, clst->cellCnt << 2, (char *)clst->cellPtr);
  
  return(Success);
}                               /* end ProcQueryColorList */


/*------------------------------------------------------------------------
----------------------- deleteFunc: DeleteColorList ----------------------
------------------------------------------------------------------------*/
int DeleteColorList(colorListPtr  clst, xieTypColorList id)
{
  if( --clst->refCnt )
    return(Success);

  /* free any colors we're holding
   */
  ResetColorList(clst, !clst->mapID ? NULL
		 : (ColormapPtr) LookupIDByType(clst->mapID, RT_COLORMAP));

  /* free the ColorList structure.
   */
  XieFree(clst);
  
  return(Success);
}                               /* end DeleteColorList */


/*------------------------------------------------------------------------
------------------------ routine: LookupColorList ------------------------
------------------------------------------------------------------------*/
colorListPtr LookupColorList(xieTypColorList id)
{
  colorListPtr clst;
  ColormapPtr  cmap;
  
  clst = (colorListPtr) LookupIDByType(id, RT_COLORLIST);
  
  if( clst && clst->mapID ) {
    /*
     *  Lookup the associated Colormap.
     */
    cmap = (ColormapPtr) LookupIDByType(clst->mapID, RT_COLORMAP);

    if( cmap != clst->mapPtr )
      /*
       *  Forget about this Colormap and the list of colors
       */
      ResetColorList(clst, cmap);
  }
  
  return(clst);
}                               /* end LookupColorList */


/*------------------------------------------------------------------------
-------------------------- routine: ResetColorList -----------------------
------------------------------------------------------------------------*/
void ResetColorList(colorListPtr clst, ColormapPtr cmap)
{
  if( clst->cellPtr ) {
    if(cmap && !clst->client->clientGone) {
      /*
       * free our colors from the colormap
       */
      FreeColors(cmap, clst->client->index, clst->cellCnt, clst->cellPtr, 0);
    }
    XieFree(clst->cellPtr);
  }

  /* reset the ColorList to its create-time state
   */
  clst->mapID   = 0;
  clst->mapPtr  = NULL;
  clst->cellCnt = 0;
  clst->cellPtr = NULL;
  clst->client  = NULL;
}                               /* end ResetColorList */


int SProcCreateColorList(ClientPtr client)
{
  register int n;
  REQUEST(xieCreateColorListReq);
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH(xieCreateColorListReq);
  swapl(&stuff->colorList, n);
  return (ProcCreateColorList(client));
}                               /* end SProcCreateColorList */

int SProcDestroyColorList(ClientPtr client)
{
  register int n;
  REQUEST( xieDestroyColorListReq );
  swaps(&stuff->length, n); 
  REQUEST_SIZE_MATCH( xieDestroyColorListReq );
  swapl(&stuff->colorList, n);
  return (ProcDestroyColorList(client));
}                               /* end SProcDestroyColorList */

int SProcPurgeColorList(ClientPtr client)
{
  register int n; 
  REQUEST( xiePurgeColorListReq );
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH( xiePurgeColorListReq );
  swapl(&stuff->colorList, n);
  return (ProcPurgeColorList(client));
}                               /* end SProcPurgeColorList */

int SProcQueryColorList(ClientPtr client)
{
  register int n;
  REQUEST( xieQueryColorListReq );
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH( xieQueryColorListReq );
  swapl(&stuff->colorList, n);
  return (ProcQueryColorList(client));
}                               /* end SProcQueryColorList */
  
/* end module colorlst.c */
