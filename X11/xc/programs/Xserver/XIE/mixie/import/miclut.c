/* $Xorg: miclut.c,v 1.4 2001/02/09 02:04:27 xorgcvs Exp $ */
/**** module miclut.c ****/
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
  
	miclut.c -- DDXIE prototype import client lut element
  
	Ben Fahy -- AGE Logic, Inc. May, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/import/miclut.c,v 3.6 2001/12/14 19:58:27 dawes Exp $ */

#define _XIEC_MICLUT
#define _XIEC_ICLUT

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
#include <lut.h>
#include <memory.h>


/*
 *  routines referenced by other DDXIE modules
 */

/*
 *  routines used internal to this module
 */

static int CreateICLUT(floDefPtr flo, peDefPtr ped);
static int InitializeICLUT(floDefPtr flo, peDefPtr ped);
static int ActivateICLUT(floDefPtr flo, peDefPtr ped, peTexPtr pet);
static int ResetICLUT(floDefPtr flo, peDefPtr ped);
static int DestroyICLUT(floDefPtr flo, peDefPtr ped);

/*
 * DDXIE ImportClientLUT entry points
 */

static ddElemVecRec ICLUTVec = {
  CreateICLUT,
  InitializeICLUT,
  ActivateICLUT,
  (xieIntProc)NULL,
  ResetICLUT,
  DestroyICLUT
  };

/*
 * Local Declarations
 */

typedef struct _lutpvt {
  int byteptr;		/* current number of bytes received */
  int bytelength;	/* selects total size of array */
  int striplength;	/* actual strip size, round up to power of 2 */
  int bytetype;		/* selects array element size */
  int bandnum;		/* for interleave, to swizzle bands */
} lutPvtRec, *lutPvtPtr;

/*------------------------------------------------------------------------
------------------- see if we can handle this element --------------------
------------------------------------------------------------------------*/
int miAnalyzeICLUT(floDefPtr flo, peDefPtr ped)
{
  /* for now just copy our entry point vector into the peDef */
  ped->ddVec = ICLUTVec;

  return(TRUE);
}

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
static int CreateICLUT(floDefPtr flo, peDefPtr ped)
{
  return MakePETex(flo,ped,xieValMaxBands * sizeof(lutPvtRec),NO_SYNC,NO_SYNC); 
}

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
static int InitializeICLUT(floDefPtr flo, peDefPtr ped)
{
  xieFloImportClientLUT *raw = (xieFloImportClientLUT *)ped->elemRaw;
  lutPvtPtr ext = (lutPvtPtr) ped->peTex->private;
  int band, nbands = ped->peTex->receptor[SRCtag].inFlo->bands;
  CARD32 *lengths = &(raw->length0);
  CARD32 *levels = &(raw->levels0);

  for (band = 0; band < nbands; band++, ext++, lengths++, levels++) {
	int deep;
	ext->byteptr = 0;
	ext->bytetype = LutPitch(*levels);
	ext->bytelength = *lengths * ext->bytetype;
	SetDepthFromLevels(ext->bytelength, deep);
	ext->striplength = (1 << deep);
	ext->bandnum = (raw->class == xieValSingleBand ||
			raw->bandOrder == xieValLSFirst)
			? band : (xieValMaxBands - band - 1);
  }
  return InitReceptors(flo,ped,NO_DATAMAP,1) &&
	 InitEmitter(flo,ped,NO_DATAMAP,NO_INPLACE);
}

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
static int ActivateICLUT(floDefPtr flo, peDefPtr ped, peTexPtr pet)
{
  int band, nbands = pet->receptor[SRCtag].inFlo->bands;
  bandPtr iband = &(pet->receptor[SRCtag].band[0]);
  lutPvtPtr ext = (lutPvtPtr) pet->private;
  
  for(band = 0; band < nbands; band++, ext++, iband++) {
    bandPtr oband = &(pet->emitter[ext->bandnum]);
    pointer ivoid, ovoid;
    int     ilen, icopy;

    if(!(pet->scheduled & 1<<band)) continue;
    if(!(ovoid = GetDstBytes(flo,pet,oband,0,ext->striplength,FALSE)))
      return(FALSE);
    /*
    **  We have no guarantee to get all the output in one packet.  In 
    **  fact we have no guarantee we will ever get all the output, or
    **  that perhaps we might get too much.  If we do get too much we
    **  would like to notify the user but the protocol doesn't provide
    **  something similar to a decodeNotify.
    */
    for(ilen = 0;
	(ivoid = GetSrcBytes(flo,pet,iband,iband->current+ilen,1,FALSE)) != 0;) {
      icopy = ilen = iband->strip->length;
      
      if ((ext->byteptr + ilen) > ext->bytelength)
	icopy = ext->bytelength - ext->byteptr;
      
      if (icopy) {
	memcpy( ((char *) ovoid) + ext->byteptr, (char *) ivoid, icopy);
	ext->byteptr += icopy;
      }
    }
    FreeData(flo, pet, iband, iband->maxLocal);
    
    /* if final, process the data received */
    if (iband->final) {
      SetBandFinal(oband);
      PutData(flo,pet,oband,ext->striplength);
    }
  }
  return TRUE;
}              

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
static int ResetICLUT(floDefPtr flo, peDefPtr ped)
{
  ResetReceptors(ped);
  ResetEmitter(ped);
  
  return(TRUE);
}

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
static int DestroyICLUT(floDefPtr flo, peDefPtr ped)
{
  /* get rid of the peTex structure  */
  ped->peTex = (peTexPtr) XieFree(ped->peTex);

  /* zap this element's entry point vector */
  ped->ddVec.create     = (xieIntProc) NULL;
  ped->ddVec.initialize = (xieIntProc) NULL;
  ped->ddVec.activate   = (xieIntProc) NULL;
  ped->ddVec.flush      = (xieIntProc) NULL;
  ped->ddVec.reset      = (xieIntProc) NULL;
  ped->ddVec.destroy    = (xieIntProc) NULL;

  return(TRUE);
}

/* end module miclut.c */

