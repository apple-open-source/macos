/* $Xorg: mefax.c,v 1.4 2001/02/09 02:04:24 xorgcvs Exp $ */
/**** module mefax.c ****/
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
  
	mefax.c -- DDXIE prototype export photomap coded with one of
			many FAX techiques (g31d,g32d,g42d,tiff2,packbits)
  
	Ben Fahy -- AGE Logic, Inc. Oct, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/export/mefax.c,v 3.6 2001/12/14 19:58:20 dawes Exp $ */

#define _XIEC_MEPHOTO
#define _XIEC_EPHOTO

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
#include <photomap.h>
#include <element.h>
#include <texstr.h>
#include <../include/fax.h>	/* XXX - ugh! 		*/
#include <../fax/fencode.h>	/* XXX - even ugh-ier! 	*/
#include <xiemd.h>
#include <memory.h>

/*
 *  routines referenced by other DDXIE modules
 */
extern int CreateEPhotoFAX(floDefPtr flo, peDefPtr ped);
extern int InitializeEPhotoFAX(floDefPtr flo, peDefPtr ped);
extern int ActivateEPhotoFAX(floDefPtr flo, peDefPtr ped, peTexPtr pet);
extern int ResetEPhotoFAX(floDefPtr flo, peDefPtr ped);
extern int DestroyEPhotoFAX(floDefPtr flo, peDefPtr ped);
extern int InitializeECPhotoFAX(floDefPtr flo, peDefPtr ped);

/* ECPhoto Create routines are shared */
/* ECPhoto Activate routines are shared */
/* ECPhoto Reset    routines are shared */
/* ECPhoto Destroy routines are shared */

/*
 * Local Declarations
 */


typedef struct _fax_encode_pvt {
  int 		 (*encodptr)();	 /* function used to encode the data 	*/
  FaxEncodeState state;		 /* holds all coding info for FAX dudes */
  int		 notify;	 /* relevant for ECPhoto only	  	*/
  int		 encoded_order;	 /* LSfirst or MSfirst			*/

  xieTypEncodeTechnique	  encode_technique;
  char 		 *tech_params;	 /* scroll away technique information	*/

  int  		 height;	 /* image height, so we know when done  */
  int  		 strip_req_newbytes;
  			 	 /* when we GetDstBytes, how much	*/
} faxPvtRec, *faxPvtPtr;

#define MaybeSwapOutput()						\
	 if (texpvt->encoded_order == xieValLSFirst) {			\
	       register int size=dbnd->maxLocal-dbnd->minLocal;		\
	       register unsigned char *ucp = dst;			\
	   	while (size--)						\
		{							\
		    *ucp = _ByteReverseTable[*ucp];			\
		    ucp++;						\
		}							\
	 }

/*
 *  routines used internal to this module
 */

static int 	common_init(
     floDefPtr flo,
     peDefPtr  ped,
     char *tec,
     CARD16 encodeTechnique);

static int 	sub_fun(
     floDefPtr flo,
     peDefPtr  ped,
     peTexPtr  pet,
     faxPvtPtr texpvt,
     FaxEncodeState *state,
     bandPtr     sbnd,
     bandPtr     dbnd);

static void FreeFaxData(floDefPtr flo, peDefPtr ped);

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
int CreateEPhotoFAX(floDefPtr flo, peDefPtr ped)
{
  /* attach an execution context to the photo element definition */
  return(MakePETex(flo, ped, sizeof(faxPvtRec), NO_SYNC, NO_SYNC));
}                               /* end CreateEPhotoFAX */
/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
int InitializeEPhotoFAX(floDefPtr flo, peDefPtr ped)
{
  ePhotoDefPtr pvt = (ePhotoDefPtr)ped->elemPvt;

  return( common_init(flo,ped,pvt->encodeParms,pvt->encodeNumber) );
}
/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
int InitializeECPhotoFAX(floDefPtr flo, peDefPtr ped)
{
  xieFloExportClientPhoto *raw = (xieFloExportClientPhoto *) ped->elemRaw;
  peTexPtr pet = ped->peTex;
  faxPvtPtr texpvt = (faxPvtPtr)pet->private;
  ePhotoDefPtr pvt = (ePhotoDefPtr)ped->elemPvt;

  if( !common_init(flo,ped,pvt->encodeParms,pvt->encodeNumber) )
    return FALSE;

  texpvt->notify = raw->notify;
  return TRUE;
}
/*------------------------------------------------------------------------
------- lots of stuff shared between ECPhoto and EPhoto. . . -------------
------------------------------------------------------------------------*/
static int common_init(
    floDefPtr flo,
    peDefPtr  ped,
    char *tec,
    CARD16 encodeTechnique)
{
peTexPtr pet = ped->peTex;
faxPvtPtr texpvt=(faxPvtPtr) pet->private;
formatPtr inf = pet->receptor[0].band[0].format;
FaxEncodeState *state = &texpvt->state;
int pbytes,max_lines_in;

/* every time we run, reset this */
   bzero(texpvt,sizeof(faxPvtRec));

/* now save away protocol technique info */
   texpvt->encode_technique = encodeTechnique;
   texpvt->tech_params 	    = tec;

/* we also need width */
   state->width   = inf->width;
   texpvt->height = inf->height;

/* do technique-dependent initialization */
   switch(encodeTechnique) {
   case xieValEncodeG31D:
   	{
	G31DEncodePvt	*epvt;
	xieTecEncodeG31D *g31dtec = (xieTecEncodeG31D *)tec;

          state->goal = ENCODE_FAX_GOAL_StartNewLine;
	  state->radiometric = g31dtec->radiometric;
	  texpvt->encoded_order = g31dtec->encodedOrder;

	  /* set up data private to this technique */
	  epvt = (G31DEncodePvt *) XieMalloc(sizeof(G31DEncodePvt));
	  bzero(epvt,sizeof(G31DEncodePvt));
	  epvt->counts = (int *) XieMalloc((state->width+1) * sizeof(int));
	  if (!epvt->counts) {
		FreeFaxData(flo,ped);
		AllocError(flo, ped, return(FALSE));
	  }
	  epvt->align_eol = g31dtec->alignEol;
	  state->private = (pointer ) epvt;

	  texpvt->encodptr = encode_g31d; 
	}
	break;
   case xieValEncodeG32D:
   	{
	G32DEncodePvt	*epvt;
	xieTecEncodeG32D *g32dtec = (xieTecEncodeG32D *)tec;

	  state->goal = ENCODE_FAX_GOAL_StartNewLine;
	  state->radiometric    = g32dtec->radiometric;
	  texpvt->encoded_order = g32dtec->encodedOrder;

	  /* set up data private to this technique */
	  epvt = (G32DEncodePvt *) XieMalloc(sizeof(G32DEncodePvt));
	  bzero(epvt,sizeof(G32DEncodePvt));
	  epvt->counts = (int *) XieMalloc((state->width+1) * sizeof(int));
	  epvt->above  = (int *) XieMalloc((state->width+1) * sizeof(int));
	  if (!epvt->counts || !epvt->above) {
		FreeFaxData(flo,ped);
		AllocError(flo, ped, return(FALSE));
	  }
	  epvt->k 		= g32dtec->kFactor;
	  epvt->align_eol 	= g32dtec->alignEol;
	  epvt->uncompressed 	= g32dtec->uncompressed;
	  state->private = (pointer ) epvt;
	}
	texpvt->encodptr = encode_g32d; 
	break;
   case xieValEncodeG42D:
   	{
	G42DEncodePvt	*epvt;
	xieTecEncodeG42D *g42dtec = (xieTecEncodeG42D *)tec;

	  state->goal = ENCODE_FAX_GOAL_StartNewLine;
	  texpvt->encoded_order = g42dtec->encodedOrder;
	  state->radiometric    = g42dtec->radiometric;

	/* set up data private to this technique */
	  epvt = (G42DEncodePvt *) XieMalloc(sizeof(G42DEncodePvt));
	  bzero(epvt,sizeof(G42DEncodePvt));
	  epvt->counts = (int *) XieMalloc((state->width+1) * sizeof(int));
	  epvt->above  = (int *) XieMalloc((state->width+1) * sizeof(int));
	  if (!epvt->counts || !epvt->above) {
		FreeFaxData(flo,ped);
		AllocError(flo, ped, return(FALSE));
	  }
	  /* for G4, initialize imaginary line to "all white" */
	  epvt->counts[0] = state->width;
	  epvt->nvals = 1;

	  epvt->uncompressed = ((xieTecEncodeG42D *)tec)->uncompressed;
	  epvt->really_g4 = 1;
	  state->private = (pointer ) epvt;
	}
	texpvt->encodptr = encode_g32d; 
	break;
   case xieValEncodeTIFF2:
   	{
	Tiff2EncodePvt	*epvt;
	xieTecEncodeTIFF2 *tiff2tec = (xieTecEncodeTIFF2 *)tec;

          state->goal = ENCODE_FAX_GOAL_StartNewLine;
	  state->radiometric    = tiff2tec->radiometric;
	  texpvt->encoded_order = tiff2tec->encodedOrder;

	  /* set up data private to this technique */
	  epvt = (Tiff2EncodePvt *) XieMalloc(sizeof(Tiff2EncodePvt));
	  bzero(epvt,sizeof(Tiff2EncodePvt));
	  epvt->counts = (int *) XieMalloc((state->width+1) * sizeof(int));
	  if (!epvt->counts) {
		FreeFaxData(flo,ped);
		AllocError(flo, ped, return(FALSE));
	  }
	  state->private = (pointer ) epvt;
	}
	texpvt->encodptr = encode_tiff2; 
	break;
   case xieValEncodeTIFFPackBits:
   	{
	PackBitsEncodePvt	*epvt;
	xieTecEncodeTIFFPackBits *tiffpbtec = (xieTecEncodeTIFFPackBits *)tec;

          state->goal = ENCODE_FAX_GOAL_StartNewLine;
	  state->width = 8 * ((state->width + 7)/8);
		/* packbits assumes lines are padded to an even byte */

	  texpvt->encoded_order = tiffpbtec->encodedOrder;

	  /* set up data private to this technique */
	  epvt = (PackBitsEncodePvt *) XieMalloc(sizeof(PackBitsEncodePvt));
	  bzero(epvt,sizeof(PackBitsEncodePvt));
	  epvt->values = (int *) XieMalloc((state->width) * sizeof(int));
	  epvt->counts = (int *) XieMalloc((state->width) * sizeof(int));
	  if (!epvt->counts || !epvt->values) {
		FreeFaxData(flo,ped);
		AllocError(flo, ped, return(FALSE));
	  }
	  state->private = (pointer ) epvt;
	}
	texpvt->encodptr = encode_tiffpb; 
	break;
   default:
        ImplementationError(flo,ped, return(FALSE));
   }

   /* size of first output strip */
   texpvt->strip_req_newbytes = flo->floTex->stripSize;
   state->strip_state = StripStateNew;

/* calculate size of the input strip data map we will need */
  pbytes = (inf->pitch + 7) >> 3;
  max_lines_in = flo->floTex->stripSize / pbytes;

  if (!max_lines_in)
	max_lines_in = 1;   /* in case a line was bigger than std stripsize */

  return(InitReceptors(flo, ped, max_lines_in, 1) &&
	 InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE));

}                               /* end common_init() */
/*------------------------------------------------------------------------
----------------------------- free some data ----------------------------
------------------------------------------------------------------------*/
static void FreeFaxData(floDefPtr flo, peDefPtr ped)
{
peTexPtr pet = ped->peTex;
faxPvtPtr texpvt=(faxPvtPtr) pet->private;
FaxEncodeState *state = &texpvt->state;

   switch(texpvt->encode_technique) {
   case xieValEncodeG31D:
	{
	   G31DEncodePvt	*epvt = (G31DEncodePvt *) state->private;
	   if (epvt)  {
	      if (epvt->counts) 
  	         XieFree(epvt->counts);
	      XieFree(epvt);
	   }
	}
	break;
   case xieValEncodeG32D:
   case xieValEncodeG42D:
	{
	   G32DEncodePvt	*epvt = (G32DEncodePvt *) state->private;
	   if (epvt)  {
	      if (epvt->counts) XieFree(epvt->counts);
	      if (epvt->above)  XieFree(epvt->above);
	      XieFree(epvt);
	   }
	}
	break;
   case xieValEncodeTIFF2:
	{
	   Tiff2EncodePvt	*epvt = (Tiff2EncodePvt *) state->private;
	   if (epvt)  {
	      if (epvt->counts) 
  	         XieFree(epvt->counts);
	      XieFree(epvt);
	   }
	}
	break;
   case xieValEncodeTIFFPackBits:
	{
	   PackBitsEncodePvt	*epvt = (PackBitsEncodePvt *) state->private;
	   if (epvt)  {
	      if (epvt->counts) XieFree(epvt->counts);
	      if (epvt->values)  XieFree(epvt->values);
	      XieFree(epvt);
	   }
	}
	break;
   default:
	break;
   }
}
/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
int ActivateEPhotoFAX(floDefPtr flo, peDefPtr ped, peTexPtr pet)
{
  receptorPtr  rcp = pet->receptor;
  bandPtr    sbnd = &rcp->band[0];
  bandPtr    dbnd = &pet->emitter[0];
  faxPvtPtr texpvt=(faxPvtPtr) pet->private;
  FaxEncodeState *state = &texpvt->state;
  int was_ready, status;
  
  if (texpvt->notify)
    was_ready = ped->outFlo.ready & 1;
    
  status = sub_fun(flo,ped,pet,texpvt,state,sbnd,dbnd);
    
  if(texpvt->notify && ~was_ready & ped->outFlo.ready & 1  &&
     (texpvt->notify==xieValNewData   ||
      (texpvt->notify==xieValFirstData && !ped->outFlo.output[0].flink->start)))
    SendExportAvailableEvent(flo,ped,0,0,0,0);
  
  return( status );
}

/*------------------------------------------------------------------------
-------------------- *really* crank some data ----------------------------
------------------------------------------------------------------------*/
static int sub_fun(
     floDefPtr flo,
     peDefPtr  ped,
     peTexPtr  pet,
     faxPvtPtr texpvt,
     FaxEncodeState *state,
     bandPtr     sbnd,
     bandPtr     dbnd)
{
BytePixel	*src,*dst;
int lines_coded;
int nl_mappable;
  
/***	This program can exit the while(dst) loop because:

	1) no new dst is available.  This would only be because the
	   scheduler wants to activate somebody else.

	2) We can't get (map) a new input strip.  Maybe it's not available
	   because our data is coming from the client.  We want to return
	   so we can get some.

	3) we finished encoding all requested lines.

	4) we notice an error.

	Now, the scheduler will always keep calling us as long as
	we either have input data or final isn't set.  We only set
	final when we have used all input data and flushed all 
	output data.  So there is no way for us to exit without
	coming back properly.
***/

    src = GetCurrentSrc(flo,pet,sbnd);

    if (dbnd->final) {
	/* be forgiving if extra data gets passed to us */
  	FreeData(flo,pet,sbnd,sbnd->maxGlobal);
	return(TRUE);
    }
    while ((dst = (BytePixel*)GetDstBytes(flo,pet,dbnd,dbnd->current,
  		texpvt->strip_req_newbytes,KEEP)) != 0) {

	if (!state->strip) {
		state->strip = dst;
		state->strip_size = dbnd->maxLocal - dbnd->current;
		state->strip_state = StripStateNew;
	}
	nl_mappable = sbnd->maxLocal - sbnd->current;
	if (nl_mappable + sbnd->current > texpvt->height)
		nl_mappable = texpvt->height - sbnd->current;

  	if (!MapData(flo,pet,sbnd,0,sbnd->current,nl_mappable,FLUSH)) {
    	     FreeData(flo,pet,sbnd,sbnd->maxLocal);
	     return(TRUE);	/* need another input strip */
	}

	state->i_lines = (char **)sbnd->dataMap;
	state->nl_tocode  = nl_mappable;

	lines_coded =  (*(texpvt->encodptr))(state);
	if (lines_coded < 0 || state->encoder_done > FAX_ENCODE_DONE_OK) {
	    /* coding error.  But we should be able to *en*code anything! */
      	    ImplementationError(flo,ped, return(FALSE));
	}
	state->i_line += lines_coded;
	sbnd->current = state->i_line;

	if (state->i_line >= texpvt->height) {
	    /* We're all done! */

	    if (state->bits.bitpos) {
		/* we need to make sure the stager gets flushed */
	       	unsigned char *byteptr= state->bits.byteptr;

	        if (state->bits.endptr > byteptr) {
		    /* there is room in this strip for the stager */
		    *byteptr = state->stager >> 24;
		}
		else {
		    /* woe is us. Stager won't fit in current output strip */
		    MaybeSwapOutput();
	            PutData(flo,pet,dbnd,dbnd->maxGlobal);
			/* flush current strip */
		    /* ask for one more strip of length 1 */
    		    dst = (BytePixel*)GetDstBytes(flo,pet,
						  dbnd,dbnd->current,1,KEEP);
		    *dst = state->stager >> 24;
		}
		state->bits.byteptr++;
	    }
	    /* however we got here, we now have one strip to flush,
	       which contains all the remaining encoded data.
	    */
	    TruncateStrip(dbnd,
			  dbnd->minLocal + state->bits.byteptr - state->strip);
	    SetBandFinal(dbnd);
	    MaybeSwapOutput();
	    PutData(flo,pet,dbnd,dbnd->maxGlobal);
  	    FreeData(flo,pet,sbnd,sbnd->maxGlobal);
	    return(TRUE);
	}

	if (state->magic_needs) {
	    /* encoder needs a new output strip */
	    if (state->strip_state != StripStateDone)
      	    	ImplementationError(flo,ped, return(FALSE));

	    MaybeSwapOutput();
	    PutData(flo,pet,dbnd,dbnd->maxGlobal);
		/* I presume this will increment dbnd->current */

	    state->strip = 0;
	    continue;
	}

    } /* end of while (GetDstBytes) */

  return(TRUE);
}                               /* end ActivateEPhotoFAX */
/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
int ResetEPhotoFAX(floDefPtr flo, peDefPtr ped)
{
  faxPvtPtr texpvt=(faxPvtPtr) ped->peTex->private;

  if (texpvt) FreeFaxData(flo,ped);

  ResetReceptors(ped);
  ResetEmitter(ped);
  
  return(TRUE);
}                               /* end ResetEPhotoFAX */

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
int DestroyEPhotoFAX(floDefPtr flo, peDefPtr ped)
{
  /* get rid of the peTex structure  */
  ped->peTex = (peTexPtr) XieFree(ped->peTex);

  /* zap this element's entry point vector */
  ped->ddVec.create     = (xieIntProc) NULL;
  ped->ddVec.initialize = (xieIntProc) NULL;
  ped->ddVec.activate   = (xieIntProc) NULL;
  ped->ddVec.reset      = (xieIntProc) NULL;
  ped->ddVec.destroy    = (xieIntProc) NULL;

  return(TRUE);
}                               /* end DestroyEPhotoFAX */
/* end module mefax.c */
