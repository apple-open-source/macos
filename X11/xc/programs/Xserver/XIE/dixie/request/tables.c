/* $Xorg: tables.c,v 1.4 2001/02/09 02:04:23 xorgcvs Exp $ */
/**** tables.c ****/
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

	tables.c: XIE vector tables

	Dean Verheiden, AGE Logic, Inc	March 1993

****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/request/tables.c,v 3.6 2001/12/14 19:58:11 dawes Exp $ */

#define _XIEC_TABLES

#define NEED_EVENTS
#define NEED_REPLIES
#include "X.h"			/* Needed for just about anything	*/
#include "Xproto.h"		/* defines protocol-related stuff	*/
#include "misc.h"		/* includes os.h, which type FatalError	*/
#include "dixstruct.h" 		/* this picks up ClientPtr definition	*/

#include "XIE.h"		
#include "XIEproto.h"		/* Xie v4 protocol specification	*/
#include <corex.h>		/* interface to core X definitions	*/
#include <error.h>
#include <tables.h>
#include <macro.h>


/*------------------------------------------------------------------------
------------------------ Procedure Not Implemented -----------------------
------------------------------------------------------------------------*/
static int ProcNotImplemented(ClientPtr client)
{
  return( BadRequest );
}                                   /* end ProcNotImplemented */


/*------------------------------------------------------------------------
---------------- Error stub for unsupported element types ----------------
------------------------------------------------------------------------*/
static peDefPtr ElementNotImplemented(
     floDefPtr      flo,
     xieTypPhototag tag,
     xieFlo        *pe)
{
  FloElementError(flo,tag,pe->elemType, return(NULL));
}

/*------------------------------------------------------------------------
---------------- Array of DD entry points called from DI  ----------------
------------------------------------------------------------------------*/
/* Very rarely, a device independent routine needs to access device dependent
   code. Interface through this array.
*/
xieVoidProc DDInterface[] = {
                (xieVoidProc)DAGalyze,
                (xieVoidProc)GetServerChoice
};


#if XIE_FULL
static int (*ProcTable5_00[])(ClientPtr) = {
/* 00 */  ProcNotImplemented,	/* Illegal protocol request */	
/* 01 */  ProcNotImplemented,	/* QueryImageExtension doesn't use the table */
/* 02 */  ProcQueryTechniques,
/* 03 */  ProcCreateColorList,
/* 04 */  ProcDestroyColorList,
/* 05 */  ProcPurgeColorList,
/* 06 */  ProcQueryColorList,
/* 07 */  ProcCreateLUT,
/* 08 */  ProcDestroyLUT,
/* 09 */  ProcCreatePhotomap,
/* 10 */  ProcDestroyPhotomap,
/* 11 */  ProcQueryPhotomap,
/* 12 */  ProcCreateROI,
/* 13 */  ProcDestroyROI,
/* 14 */  ProcCreatePhotospace,
/* 15 */  ProcDestroyPhotospace,
/* 16 */  ProcExecuteImmediate,
/* 17 */  ProcCreatePhotoflo,
/* 18 */  ProcDestroyPhotoflo,
/* 19 */  ProcExecutePhotoflo,
/* 20 */  ProcModifyPhotoflo,
/* 21 */  ProcRedefinePhotoflo,
/* 22 */  ProcPutClientData,
/* 23 */  ProcGetClientData,
/* 24 */  ProcQueryPhotoflo,
/* 25 */  ProcAwait,
/* 26 */  ProcAbort
  };

static int (*SProcTable5_00[])(ClientPtr) = {
/* 00 */  ProcNotImplemented,	/* Illegal protocol request */	
/* 01 */  ProcNotImplemented,	/* QueryImageExtension doesn't use the table */
/* 02 */  SProcQueryTechniques,
/* 03 */  SProcCreateColorList,
/* 04 */  SProcDestroyColorList,
/* 05 */  SProcPurgeColorList,
/* 06 */  SProcQueryColorList,
/* 07 */  SProcCreateLUT,
/* 08 */  SProcDestroyLUT,
/* 09 */  SProcCreatePhotomap,
/* 10 */  SProcDestroyPhotomap,
/* 11 */  SProcQueryPhotomap,
/* 12 */  SProcCreateROI,
/* 13 */  SProcDestroyROI,
/* 14 */  SProcCreatePhotospace,
/* 15 */  SProcDestroyPhotospace,
/* 16 */  SProcExecuteImmediate,
/* 17 */  SProcCreatePhotoflo,
/* 18 */  SProcDestroyPhotoflo,
/* 19 */  SProcExecutePhotoflo,
/* 20 */  SProcModifyPhotoflo,
/* 21 */  SProcRedefinePhotoflo,
/* 22 */  SProcPutClientData,
/* 23 */  SProcGetClientData,
/* 24 */  SProcQueryPhotoflo,
/* 25 */  SProcAwait,
/* 26 */  SProcAbort
  };
     
peDefPtr (*MakeTable[])(
     floDefPtr      flo,
     xieTypPhototag tag,
     xieFlo        *pe) = {
/* 00 */  ElementNotImplemented,
/* 01 */  MakeICLUT,
/* 02 */  MakeICPhoto,
/* 03 */  MakeICROI,
/* 04 */  MakeIDraw,
/* 05 */  MakeIDrawP,
/* 06 */  MakeILUT,
/* 07 */  MakeIPhoto,
/* 08 */  MakeIROI,
/* 09 */  MakeArith,
/* 10 */  MakeBandCom,
/* 11 */  MakeBandExt,
/* 12 */  MakeBandSel,
/* 13 */  MakeBlend,
/* 14 */  MakeCompare,
/* 15 */  MakeConstrain,
/* 16 */  MakeConvertFromIndex,
/* 17 */  MakeConvertFromRGB,
/* 18 */  MakeConvertToIndex,
/* 19 */  MakeConvertToRGB,
/* 20 */  MakeConvolve,
/* 21 */  MakeDither,
/* 22 */  MakeGeometry,
/* 23 */  MakeLogic,
/* 24 */  MakeMatchHistogram,
/* 25 */  MakeMath,
/* 26 */  MakePasteUp,
/* 27 */  MakePoint,
/* 28 */  MakeUnconstrain,
/* 29 */  MakeECHistogram,
/* 30 */  MakeECLUT,
/* 31 */  MakeECPhoto,
/* 32 */  MakeECROI,
/* 33 */  MakeEDraw,
/* 34 */  MakeEDrawPlane,
/* 35 */  MakeELUT,
/* 36 */  MakeEPhoto,
/* 37 */  MakeEROI
  };

#else /* XIEdis */

static int (*ProcTable5_00[])() = {
/* 00 */  ProcNotImplemented,	/* Illegal protocol request */	
/* 01 */  ProcNotImplemented,	/* QueryImageExtension doesn't use the table */
/* 02 */  ProcQueryTechniques,
/* 03 */  ProcNotImplemented,		/* CreateColorList */
/* 04 */  ProcNotImplemented,		/* DestroyColorList */
/* 05 */  ProcNotImplemented,		/* PurgeColorList */
/* 06 */  ProcNotImplemented,		/* QueryColorList */
/* 07 */  ProcCreateLUT,
/* 08 */  ProcDestroyLUT,
/* 09 */  ProcCreatePhotomap,
/* 10 */  ProcDestroyPhotomap,
/* 11 */  ProcQueryPhotomap,
/* 12 */  ProcNotImplemented,		/* CreateROI */
/* 13 */  ProcNotImplemented,		/* DestroyROI */
/* 14 */  ProcCreatePhotospace,
/* 15 */  ProcDestroyPhotospace,
/* 16 */  ProcExecuteImmediate,
/* 17 */  ProcCreatePhotoflo,
/* 18 */  ProcDestroyPhotoflo,
/* 19 */  ProcExecutePhotoflo,
/* 20 */  ProcModifyPhotoflo,
/* 21 */  ProcRedefinePhotoflo,
/* 22 */  ProcPutClientData,
/* 23 */  ProcGetClientData,
/* 24 */  ProcQueryPhotoflo,
/* 25 */  ProcAwait,
/* 26 */  ProcAbort
  };

static int (*SProcTable5_00[])() = {
/* 00 */  ProcNotImplemented,	/* Illegal protocol request */	
/* 01 */  ProcNotImplemented,	/* QueryImageExtension doesn't use the table */
/* 02 */  SProcQueryTechniques,
/* 03 */  ProcNotImplemented,		/* CreateColorList */
/* 04 */  ProcNotImplemented,		/* DestroyColorList */
/* 05 */  ProcNotImplemented,		/* PurgeColorList */
/* 06 */  ProcNotImplemented,		/* QueryColorList */
/* 07 */  SProcCreateLUT,
/* 08 */  SProcDestroyLUT,
/* 09 */  SProcCreatePhotomap,
/* 10 */  SProcDestroyPhotomap,
/* 11 */  SProcQueryPhotomap,
/* 12 */  ProcNotImplemented,		/* CreateROI */
/* 13 */  ProcNotImplemented,		/* DestroyROI */
/* 14 */  SProcCreatePhotospace,
/* 15 */  SProcDestroyPhotospace,
/* 16 */  SProcExecuteImmediate,
/* 17 */  SProcCreatePhotoflo,
/* 18 */  SProcDestroyPhotoflo,
/* 19 */  SProcExecutePhotoflo,
/* 20 */  SProcModifyPhotoflo,
/* 21 */  SProcRedefinePhotoflo,
/* 22 */  SProcPutClientData,
/* 23 */  SProcGetClientData,
/* 24 */  SProcQueryPhotoflo,
/* 25 */  SProcAwait,
/* 26 */  SProcAbort
  };
     
peDefPtr (*MakeTable[])() = {
/* 00 */  ElementNotImplemented,
/* 01 */  MakeICLUT,
/* 02 */  MakeICPhoto,
/* 03 */  ElementNotImplemented,	/* ICROI */
/* 04 */  MakeIDraw,
/* 05 */  MakeIDrawP,
/* 06 */  MakeILUT,
/* 07 */  MakeIPhoto,
/* 08 */  ElementNotImplemented,	/* IROI */
/* 09 */  ElementNotImplemented,	/* Arith */
/* 10 */  ElementNotImplemented,	/* BandCom */
/* 11 */  ElementNotImplemented,	/* BandExt */
/* 12 */  ElementNotImplemented,	/* BandSel */
/* 13 */  ElementNotImplemented,	/* Blend */
/* 14 */  ElementNotImplemented,	/* Compare */
/* 15 */  ElementNotImplemented,	/* Constrain */
/* 16 */  ElementNotImplemented,	/* ConvertFromIndex */
/* 17 */  ElementNotImplemented,	/* ConvertFromRGB */
/* 18 */  ElementNotImplemented,	/* ConvertToIndex */
/* 19 */  ElementNotImplemented,	/* ConvertToRGB */
/* 20 */  ElementNotImplemented,	/* Convolve */
/* 21 */  ElementNotImplemented,	/* Dither */
/* 22 */  MakeGeometry,
/* 23 */  ElementNotImplemented,	/* Logic */
/* 24 */  ElementNotImplemented,	/* MatchHistogram */
/* 25 */  ElementNotImplemented,	/* Math */
/* 26 */  ElementNotImplemented,	/* PasteUp */
/* 27 */  MakePoint,
/* 28 */  ElementNotImplemented,	/* Unconstrain */
/* 29 */  ElementNotImplemented,	/* ECHistogram */
/* 30 */  MakeECLUT,
/* 31 */  MakeECPhoto,
/* 32 */  ElementNotImplemented,	/* ECROI */
/* 33 */  MakeEDraw,
/* 34 */  MakeEDrawPlane,
/* 35 */  MakeELUT,
/* 36 */  MakeEPhoto,
/* 37 */  ElementNotImplemented		/* EROI */
  };
#endif
   
/************************************************************************
     Fill in the version specific tables for the selected minor protocol 
     version
************************************************************************/
void init_proc_tables(
     CARD16 minorVersion,
     int (**ptable[])(ClientPtr),
     int (**sptable[])(ClientPtr))
{
  /* Kind of boring with only one version to work with */
  switch (minorVersion) {
  case 0:
    *ptable  =  ProcTable5_00;
    *sptable = SProcTable5_00;
    break;
  default:
    *ptable  =  ProcTable5_00;
    *sptable = SProcTable5_00;
  }
}

/* end module tables.c */
