/* $Xorg: error.c,v 1.6 2001/02/09 02:04:22 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module error.c ****/
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
/* $XFree86: xc/programs/Xserver/XIE/dixie/request/error.c,v 3.5 2001/12/14 19:58:09 dawes Exp $ */

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
 *  XIE includes
 */
#include <XIE.h>
#include <XIEproto.h>
/*
 *  more X server includes.
 */
#include <misc.h>
#include <extnsionst.h>
#include <dixstruct.h>
#include <opaque.h>

#include <corex.h>
#include <macro.h>
#include <flostr.h>

/*
 *  routines referenced by other modules.
 */


/*------------------------------------------------------------------------
-------------------------- Send Resource Error ---------------------------
------------------------------------------------------------------------*/
int
SendResourceError(ClientPtr client, CARD8 code, XID id)
{
  xieResourceErr err;
  REQUEST(xieReq);

  if(client->clientGone) return(Success);
  
  bzero((char *)&err, sz_xieResourceErr);
  err.error       = X_Error;
  err.code        = code + extEntry->errorBase;
  err.sequenceNum = client->sequence;
  err.resourceID  = id;
  err.minorOpcode = stuff->opcode;
  err.majorOpcode = stuff->reqType;

  if( client->swapped ) {
    register int n;
    swaps(&err.sequenceNum,n);
    swapl(&err.resourceID,n);
    swaps(&err.minorOpcode,n);
  }
  WriteToClient(client, sz_xieResourceErr, (char *)&err);
  isItTimeToYield = TRUE;

  return(Success);
}                               /* end SendResourceError */

/*------------------------------------------------------------------------
----------------------------- Send FloID Error ---------------------------
------------------------------------------------------------------------*/
int
SendFloIDError(ClientPtr client, XID spaceID, XID floID)
{
  xieFloIDErr err;
  REQUEST(xieReq);
  
  if(client->clientGone) return(Success);

  bzero((char *)&err, sz_xieFloErr);
  err.error        = X_Error;
  err.code         = xieErrNoFlo + extEntry->errorBase;
  err.sequenceNum  = client->sequence;
  err.floID        = floID;
  err.floErrorCode = xieErrNoFloID;
  err.minorOpcode  = stuff->opcode;
  err.majorOpcode  = stuff->reqType;
  err.nameSpace    = spaceID;
  
  if( client->swapped ) {
    register int n;
    swaps(&err.sequenceNum, n);
    swapl(&err.nameSpace, n);
    swapl(&err.floID, n);
    swaps(&err.minorOpcode,n);
  }
  WriteToClient(client, sz_xieFloErr, (char *)&err);
  isItTimeToYield = TRUE;
  
  return(Success);
}                               /* end SendFloIDError */

/*------------------------------------------------------------------------
----------------------------- Send Flo Error -----------------------------
------------------------------------------------------------------------*/
int
SendFloError(ClientPtr client, floDefPtr flo)
{
  int status = Success;
  register int n;
  xieFloErr err;
  REQUEST(xieReq);
  
  if(client->clientGone) return(Success);
  /*
   * Take care of the common part
   */
  err = flo->error;
  err.error 	  = X_Error;
  err.code        = xieErrNoFlo + extEntry->errorBase;
  err.sequenceNum = client->sequence;
  err.floID       = flo->ID;
  err.minorOpcode = stuff->opcode;
  err.majorOpcode = stuff->reqType;
  err.nameSpace   = flo->spaceID;
  
  if( client->swapped ) {
    swaps(&err.sequenceNum, n);
    swapl(&err.nameSpace, n);
    swapl(&err.floID, n);
    swaps(&err.phototag, n);
    swaps(&err.type, n);
    swaps(&err.minorOpcode,n);
  }
  /* take care of the unique parts
   */
  switch( ferrCode(flo) ) {
  case	xieErrNoFloAccess:
  case	xieErrNoFloAlloc:
  case	xieErrNoFloElement:
  case	xieErrNoFloImplementation:
  case	xieErrNoFloLength:
  case	xieErrNoFloMatch:
  case	xieErrNoFloOperator:
  case	xieErrNoFloSource:
    break;
  case	xieErrNoFloColormap:
  case	xieErrNoFloColorList:
  case	xieErrNoFloDrawable:
  case	xieErrNoFloGC:
  case	xieErrNoFloLUT:
  case	xieErrNoFloPhotomap:
  case	xieErrNoFloROI:
    if( client->swapped ) {
      swapl(&((xieFloResourceErr *)(&err))->resourceID, n);
    }
    break;
  case	xieErrNoFloDomain:
    if( client->swapped ) {
      swaps(&((xieFloDomainErr *)(&err))->domainSrc, n);
    }
    break;
  case	xieErrNoFloTechnique:
    if( client->swapped ) {
      swaps(&((xieFloTechniqueErr *)(&err))->techniqueNumber, n);
      swaps(&((xieFloTechniqueErr *)(&err))->lenTechParams, n);
    }
    break;
  case	xieErrNoFloValue:
    if( client->swapped ) {
      swapl(&((xieFloValueErr *)(&err))->badValue, n);
    }
    break;
  default:
    status = BadImplementation;
    break;
  }
  
  if( status == Success ) {
    WriteToClient(client, sz_xieFloErr, (char *)&err);
    isItTimeToYield = TRUE;
  }
  return(status);
}                               /* end SendFloError */


/*------------------------------------------------------------------------
-------------- Convenience routines for setting Flo Errors ---------------
------------------------------------------------------------------------*/
void
FloError(floDefPtr flo, xieTypPhototag tag, CARD16 type, CARD8 code)
{
  ferrError(flo,tag,type,code);
}

void
ErrGeneric(floDefPtr flo, peDefPtr ped, CARD8 code)
{
  ferrError(flo,ped->phototag,ped->elemRaw->elemType,code);
}

void
ErrResource(floDefPtr flo, peDefPtr ped, CARD8 code, CARD32 id)
{
  ferrError(flo,ped->phototag,ped->elemRaw->elemType,code);
  ((xieFloResourceErr *)(&flo->error))->resourceID = id;
}

void
ErrDomain(floDefPtr flo, peDefPtr ped, xieTypPhototag domain)
{
  ferrError(flo,ped->phototag,ped->elemRaw->elemType,xieErrNoFloDomain);
  ((xieFloDomainErr *)(&flo->error))->domainSrc = domain;
}

void
ErrOperator(floDefPtr flo, peDefPtr ped, CARD8 operator)
{
  ferrError(flo,ped->phototag,ped->elemRaw->elemType,xieErrNoFloOperator);
  ((xieFloOperatorErr *)(&flo->error))->operator = operator;
}

void
ErrTechnique(floDefPtr flo, peDefPtr ped, CARD8 group, CARD16 tech,
	     CARD16 lenParams)
{
  ferrError(flo,ped->phototag,ped->elemRaw->elemType,xieErrNoFloTechnique);
  ((xieFloTechniqueErr *)(&flo->error))->techniqueGroup  = group;
  ((xieFloTechniqueErr *)(&flo->error))->techniqueNumber = tech;
  ((xieFloTechniqueErr *)(&flo->error))->lenTechParams   = lenParams;
}

void
ErrValue(floDefPtr flo, peDefPtr ped, CARD32 value)
{
  ferrError(flo,ped->phototag,ped->elemRaw->elemType,xieErrNoFloValue);
  ((xieFloValueErr *)(&flo->error))->badValue = value;
}

/* end module error.c */
