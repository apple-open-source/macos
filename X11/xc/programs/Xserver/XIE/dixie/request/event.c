/* $Xorg: event.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/**** module event.c ****/
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
  
	event.c -- DIXIE routines for managing events
  
	Dean Verheiden -- AGE Logic, Inc. April 1993

****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/request/event.c,v 3.5 2001/12/14 19:58:09 dawes Exp $ */

#define _XIEC_EVENT

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

#include <corex.h>
#include <macro.h>
#include <flostr.h>

/*------------------------------------------------------------------------
----------------------------- Send Flo Event -----------------------------
------------------------------------------------------------------------*/
void SendFloEvent(floDefPtr flo)
{
  int status = Success;
  register int n;
  xieFloEvn evn;
  
  if(flo->runClient->clientGone) return;
  /*
   * Take care of the common part
   */
  evn = flo->event;
  evn.sequenceNum 	= flo->runClient->sequence;
  evn.time		= currentTime.milliseconds;
  evn.instanceNameSpace = flo->spaceID;
  evn.instanceFloID	= flo->ID;
  
  if( flo->runClient->swapped ) {
    swaps(&evn.sequenceNum, n);
    swapl(&evn.time, n);
    swapl(&evn.instanceNameSpace, n);
    swapl(&evn.instanceFloID, n);
    /*
     * Take care of the unique parts 
     */
    switch( evn.event ) {
    case xieEvnNoColorAlloc:
      swaps(&evn.src, n);
      swaps(&evn.type, n);
      swapl(&((xieColorAllocEvn *)&evn)->colorList, n);
      swaps(&((xieColorAllocEvn *)&evn)->colorAllocTechnique, n);
      swapl(&((xieColorAllocEvn *)&evn)->data, n);
      break;
    case xieEvnNoDecodeNotify:
      swaps(&evn.src, n);
      swaps(&evn.type, n);
      swaps(&((xieDecodeNotifyEvn *)&evn)->decodeTechnique, n);
      swapl(&((xieDecodeNotifyEvn *)&evn)->width, n);
      swapl(&((xieDecodeNotifyEvn *)&evn)->height, n);
      break;
    case xieEvnNoExportAvailable:
      swaps(&evn.src, n);
      swaps(&evn.type, n);
      swapl(&((xieExportAvailableEvn *)&evn)->data0, n);
      swapl(&((xieExportAvailableEvn *)&evn)->data1, n);
      swapl(&((xieExportAvailableEvn *)&evn)->data2, n);
      break;
    case xieEvnNoImportObscured:
      swaps(&evn.src, n);
      swaps(&evn.type, n);
      swapl(&((xieImportObscuredEvn *)&evn)->window, n);
      swaps(&((xieImportObscuredEvn *)&evn)->x, n);
      swaps(&((xieImportObscuredEvn *)&evn)->y, n);
      swaps(&((xieImportObscuredEvn *)&evn)->width, n);
      swaps(&((xieImportObscuredEvn *)&evn)->height, n);
      break;
    case xieEvnNoPhotofloDone:
      break;
    default:
      status = BadImplementation;
      break;
    }
  }
  /* add in our event base */
  evn.event += extEntry->eventBase;
  
  if( status == Success )
    WriteToClient(flo->runClient, sz_xieFloEvn, (char *)&evn);
  
}                               /* end SendFloEvent */

/* end module event.c */
