/* $Xorg: lut.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/**** module lut.c ****/
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
****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/request/lut.c,v 3.5 2001/12/14 19:58:10 dawes Exp $ */

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
 *  Module Specific Includes
 */
#include <corex.h>
#include <macro.h>
#include <memory.h>
#include <flodata.h>
#include <lut.h>
#include <tables.h>

/*
 *  routines referenced by other modules.
 */
extern int DeleteLUT(lutPtr lut, xieTypLUT id);


/*------------------------------------------------------------------------
--------------------------- CreateLUT Procedures -------------------------
------------------------------------------------------------------------*/
int ProcCreateLUT(ClientPtr client)
{
  int b;
  lutPtr lut;
  REQUEST(xieCreateLUTReq);
  REQUEST_SIZE_MATCH(xieCreateLUTReq);
  LEGAL_NEW_RESOURCE(stuff->lut, client);

  /* create a new lookup table
   */
  if( !(lut = (lutPtr) XieCalloc(sizeof(lutRec))) )
    return(client->errorValue = stuff->lut, BadAlloc);
  
  lut->ID	= stuff->lut;
  lut->refCnt	= 1;
  for(b = 0; b < xieValMaxBands; b++)
    ListInit(&lut->strips[b]);

  return( AddResource(lut->ID, RT_LUT, (lutPtr)lut)
	? Success : (client->errorValue = stuff->lut, BadAlloc) );
}                               /* end ProcCreateLUT */


/*------------------------------------------------------------------------
------------------------ DestroyLUT Procedures --------------------------
------------------------------------------------------------------------*/
int ProcDestroyLUT(ClientPtr client)
{
  lutPtr lut;
  REQUEST( xieDestroyLUTReq );
  REQUEST_SIZE_MATCH( xieDestroyLUTReq );
  
  if( !(lut = (lutPtr)LookupIDByType(stuff->lut, RT_LUT)) ) 
    return( SendResourceError(client, xieErrNoLUT, stuff->lut) );
  
  /* Disassociate the LUT from core X -- it calls DeleteLUT()
   */
  FreeResourceByType(stuff->lut, RT_LUT, RT_NONE);
  
  return(Success);
}                               /* end ProcDestroyLUT */


/*------------------------------------------------------------------------
------------------------ deleteFunc: DeleteLUT ---------------------------
------------------------------------------------------------------------*/
int DeleteLUT(lutPtr lut, xieTypLUT id)
{
  int i;
  
  if( --lut->refCnt )
    return(Success);
  
  /* Free any lookup table arrays 
   */
  for(i = 0; i < lut->lutCnt; i++ )
    FreeStrips(&lut->strips[i]);

  /* Free the LUT structure.
   */
  XieFree(lut);
  
  return(Success);
}                               /* end DeleteLUT */


int SProcCreateLUT(ClientPtr client)
{
  register int n;
  REQUEST(xieCreateLUTReq);
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH(xieCreateLUTReq);
  swapl(&stuff->lut, n);
  return (ProcCreateLUT(client));
}                               /* end SProcCreateLUT */

int SProcDestroyLUT(ClientPtr client)
{
  register int n;
  REQUEST( xieDestroyLUTReq );
  swaps(&stuff->length, n);
  REQUEST_SIZE_MATCH( xieDestroyLUTReq );
  swapl(&stuff->lut, n);
  return (ProcDestroyLUT(client));
}                               /* end SProcDestroyLUT */

/* end module LUT.c */
