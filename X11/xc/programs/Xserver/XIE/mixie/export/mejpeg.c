/* $Xorg: mejpeg.c,v 1.4 2001/02/09 02:04:24 xorgcvs Exp $ */
/**** module mejpeg.c ****/
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
  
	mejpeg.c -- DDXIE prototype export photomap coded ala JPEG element
  
	Ben Fahy -- AGE Logic, Inc. Oct, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/export/mejpeg.c,v 3.8 2001/12/14 19:58:20 dawes Exp $ */

#define _XIEC_MEPHOTO
#define _XIEC_EPHOTO
#define _XIEC_ECPHOTO

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
#include <jpeg.h>
#include <photomap.h>
#include <element.h>
#include <texstr.h>
#include <memory.h>


/*
 *  routines referenced by other DDXIE modules
 */
extern int CreateEPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped);
extern int InitializeEPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped);
extern int ActivateEPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped, peTexPtr pet);
extern int ResetEPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped);
extern int DestroyEPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped);

/* Create routines are shared */
int InitializeECPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped);
/* Activate routines are shared */
/* Reset    routines are shared */
/* Destroy  routines are shared */

/*
 *  routines used internal to this module
 */

/*
 * Local Declarations
 */

typedef struct _jpeg_encode_pvt {
  int 	(*encodptr)();		/* function used to encode the data 	*/

  xieTypDataClass 	class;		/* SingleBand or TripleBand 	  */
  int			out_bands;	/* should be 1 if interleaved	  */
  int			 in_bands;	/* should be 3 if TripleBand	  */
  int			colors_smushed;	/* TripleBand BandByPixel	  */
  int			swizzle;	/* true to reverse band order     */
  int			notify;		/* relevant for ECPhoto only	  */
  /* the following are used by the JPEG private routines */
  struct Compress_methods_struct c_methods;
  struct External_methods_struct e_methods;

  /* these things hold state. may need one for each band */
  JpegEncodeState		state[3];
  struct Compress_info_struct 	cinfo[3];
  unsigned char 		output_buffer[3][JPEG_BUF_SIZE];
} jpegPvtRec, *jpegPvtPtr;

static int sub_fun(
     floDefPtr flo,
     peDefPtr  ped,
     peTexPtr  pet,
     jpegPvtPtr texpvt,
     JpegEncodeState *state,
     bandPtr     sbnd,
     bandPtr     dbnd,
     bandPtr     sbnd1,
     bandPtr     sbnd2);

static int common_init(
     floDefPtr flo,
     peDefPtr  ped,
     xieTecEncodeJPEGBaseline *tec,
     CARD16    encodeTechnique);

static int FlushJpegEncodeData(
     bandPtr           dbnd,
     register unsigned char *dst,
     JpegEncodeState *state);

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
int CreateEPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped)
{
  /* attach an execution context to the photo element definition */
  return(MakePETex(flo, ped, sizeof(jpegPvtRec), NO_SYNC, NO_SYNC));
}                               /* end CreateEPhotoJPEGBaseline */
/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
int InitializeEPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped)
{
  ePhotoDefPtr pvt = (ePhotoDefPtr)ped->elemPvt;

  return( common_init(flo,ped,pvt->encodeParms,pvt->encodeNumber) );
}
/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
int InitializeECPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped)
{
  ePhotoDefPtr pvt = (ePhotoDefPtr)ped->elemPvt;
  peTexPtr pet = ped->peTex;
  jpegPvtPtr texpvt=(jpegPvtPtr) pet->private;

  if(!common_init(flo,ped,pvt->encodeParms,pvt->encodeNumber)) {
    if(ferrCode(flo))
      return(FALSE);
    else
      TechniqueError(flo,ped,xieValEncode,
		     ((xieFloExportClientPhoto*)ped->elemRaw)->encodeTechnique,
		     ((xieFloExportClientPhoto*)ped->elemRaw)->lenParams,
		     return(FALSE));
  }
  texpvt->notify = ((xieFloExportClientPhoto *)ped->elemRaw)->notify;
  return(TRUE);
}
/*------------------------------------------------------------------------
------- lots of stuff shared between ECPhoto and EPhoto. . . -------------
------------------------------------------------------------------------*/
static int common_init(
     floDefPtr flo,
     peDefPtr  ped,
     xieTecEncodeJPEGBaseline *tec,
     CARD16    encodeTechnique)
{
  peTexPtr pet = ped->peTex;
  eTecEncodeJPEGBaselineDefPtr pedpvt=(eTecEncodeJPEGBaselineDefPtr) 
						ped->techPvt;
  jpegPvtPtr texpvt=(jpegPvtPtr) pet->private;
  int     out_bands = ped->outFlo.bands;	   /* # of output bands */
  int      in_bands = ped->inFloLst[SRCtag].bands; /* # of  input bands */
  formatPtr inf = pet->receptor[0].band[0].format;
  compress_info_ptr cinfo;
  int pbytes,max_lines_in,b;
  
  /* every time we run, reset this */
  bzero(texpvt,sizeof(jpegPvtRec));
  
  /* squirrel away # of bands and class output, class of input */
  texpvt->in_bands  =  in_bands;
  texpvt->out_bands = out_bands;
  texpvt->class = (in_bands == 3)? xieValTripleBand : xieValSingleBand;
  texpvt->colors_smushed = 0;
  
  /* note: we assume dixie side has set up the in/out #bands properly */
  if (in_bands == 1) 
    texpvt->encodptr = encode_jpeg_lossy_gray;
				/* JPEG will be coding grayscale */

  else  {
    /* if interleave is BandByPlane, do gray, one band at a time.
     * otherwise, do color, three bands at a time.
     */
    if (in_bands == out_bands) {  /* interleaving BandByPlane */
      texpvt->encodptr = encode_jpeg_lossy_gray;
				/* JPEG will code each band individually */
    }
    else  { /* BandByPixel, do all 3 bands at once  */
      texpvt->encodptr = encode_jpeg_lossy_color; 
      texpvt->colors_smushed = 1;
				/* JPEG will code all bands simultaneously */
    }
  }
  ped->peTex->bandSync  = in_bands != out_bands;
  texpvt->swizzle = tec->bandOrder == xieValMSFirst;
  
  /* now deal with stuff on per-band basis */
  for (b=0; b < out_bands; ++b) {
    JpegEncodeState *state = &(texpvt->state[b]);
    
    state->width  = inf->width;
    state->height = inf->height;
    state->n_bands = texpvt->colors_smushed ?  3 : 1;
    /* this is how many bands the encoder looks at. If the image is	*/
    /* TripleBand-BandByPixel, 3.  If BandByPlane or SingleBand, 1 	*/
    
    state->c_methods = &texpvt->c_methods;
    state->e_methods = &texpvt->e_methods;
    
    state->lenQtable  	= tec->lenQtable;
    state->lenACtable 	= tec->lenACtable;
    state->lenDCtable 	= tec->lenDCtable;
    state->Qtable 	= pedpvt->q;
    state->ACtable 	= pedpvt->a;
    state->DCtable 	= pedpvt->d;

    state->goal = JPEG_ENCODE_GOAL_Startup;
    state->needs_input_strip = 1;
    
    cinfo = state->cinfo = &texpvt->cinfo[b];
    if(JC_INIT(cinfo,state->c_methods,state->e_methods) != 0)
      return(FALSE);

    cinfo->jpeg_buf_size = JPEG_BUF_SIZE;
    cinfo->output_buffer = (char *) texpvt->output_buffer[b];
    state->jpeg_output_buffer =     texpvt->output_buffer[b];
    
    /* size of first output strip */
    state->strip_req_newbytes = flo->floTex->stripSize;

    if(texpvt->colors_smushed) {
      int j;
      for(j = 0; j < xieValMaxBands; ++j) {
	state->h_sample[j] = tec->horizontalSamples[j];
	state->v_sample[j] = tec->verticalSamples[j];
      }
    }
  }
  /* calculate size of the input strip data map we will need */
  pbytes = (inf->pitch + 7) >> 3;
  max_lines_in = flo->floTex->stripSize / pbytes;
  
  if (!max_lines_in)
    max_lines_in = 1;   /* in case a line was bigger than std stripsize */
  
  return(InitReceptors(flo, ped, max_lines_in, 1) &&
	 InitEmitter(flo, ped, NO_DATAMAP, NO_INPLACE));
  
}                               /* end common_init() */


/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
int ActivateEPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped, peTexPtr pet)
{
  receptorPtr  rcp = pet->receptor;
  bandPtr    sbnd  = &rcp->band[0];	/* "red" (or gray) input */
  bandPtr    sbnd1 = &rcp->band[1];	/* optional "green" 	 */
  bandPtr    sbnd2 = &rcp->band[2];	/* optional "blue"	 */
  bandPtr    dbnd  = &pet->emitter[0];
  jpegPvtPtr texpvt=(jpegPvtPtr) pet->private;
  int b, d, was_ready = 0, status;
  
  /* if the class of the src is SingleBand, we call sub_fun for band 0 	*/
  /* if the class of the src is TripleBand and the interleave is 	*/
  /* 	BandByPlane, we call sub_fun 3 times, once for each band	*/
  /* if the class of the src is TripleBand, interleave BandByPixel, we	*/
  /* 	call subfun once, supplying it all three source bands		*/
  
  if (texpvt->class == xieValSingleBand) {
    JpegEncodeState *state = &(texpvt->state[0]);
    
    if (texpvt->notify) {
      was_ready = ped->outFlo.ready;
    }
    
    status = sub_fun(flo,ped,pet,texpvt,state,sbnd,dbnd,NULL,NULL);
    
    if(texpvt->notify && ~was_ready & ped->outFlo.ready & 1  &&
       (texpvt->notify == xieValNewData   ||
	(texpvt->notify == xieValFirstData &&
	!ped->outFlo.output[0].flink->start)))
      SendExportAvailableEvent(flo,ped,0,0,0,0);
    
    return( status );
  }
  
  /* TripleBand */
  if (texpvt->colors_smushed) {				/* BandByPixel */
    JpegEncodeState *state = &(texpvt->state[0]);
    
    if (texpvt->notify) {
      was_ready = ped->outFlo.ready & 1;
    }
    if(texpvt->swizzle)
      status = sub_fun(flo,ped,pet,texpvt,state,sbnd2,dbnd,sbnd1,sbnd);
    else
      status = sub_fun(flo,ped,pet,texpvt,state,sbnd,dbnd,sbnd1,sbnd2);
    
    if(texpvt->notify && ~was_ready & ped->outFlo.ready & 1 &&
       (texpvt->notify == xieValNewData   ||
	(texpvt->notify == xieValFirstData &&
	!ped->outFlo.output[0].flink->start)))
      SendExportAvailableEvent(flo,ped,0,0,0,0);
    
    return( status );
  }
  
  /* TripleBand, BandByPlane */
  for(b = 0; b < xieValMaxBands; ++b) {
    JpegEncodeState *state = &(texpvt->state[b]);
    d    = texpvt->swizzle ? xieValMaxBands - b - 1 : b;
    sbnd = &rcp->band[b];	/* do each band independently */
    dbnd = &pet->emitter[d];
    
    if (texpvt->notify) {
      was_ready = ped->outFlo.ready & 1<<d;
    }
    
    status = sub_fun(flo,ped,pet,texpvt,state,sbnd,dbnd,NULL,NULL);
    
    if(texpvt->notify && ~was_ready & ped->outFlo.ready & 1<<d &&
       (texpvt->notify == xieValNewData   ||
	(texpvt->notify == xieValFirstData &&
	!ped->outFlo.output[d].flink->start)))
      SendExportAvailableEvent(flo,ped,d,0,0,0);
    
    if (status == FALSE)
      return( status );
  }
  return(TRUE);
}
/*------------------------------------------------------------------------
-------------------- *really* crank some data ----------------------------
------------------------------------------------------------------------*/
static int sub_fun(
     floDefPtr flo,
     peDefPtr  ped,
     peTexPtr  pet,
     jpegPvtPtr texpvt,
     JpegEncodeState *state,
     bandPtr     sbnd,
     bandPtr     dbnd,
     bandPtr     sbnd1,
     bandPtr     sbnd2)
{
BytePixel	*dst;
int status;
  
/***	This program can return due to the following reasons:

	1) we have provided all the data an input strip has to the 
	   encoder, and we are not at the end of the image.  In this
	   case we can expect to return when more data comes.

	2) We try to get another Destination strip,  but GetDst
	   turns us down because the scheduler wants to activate 
	   somebody else.  Since we haven't set final yet, we
	   are ok.

	3) after encoding, we notice the state is Done and there 
		is no data left to flush. We set final and return.

	4) we finish flushing all of our data,  and notice state 
		is Done. We set final and return.

	5) we notice an error.

	Now, the scheduler will always keep calling us as long as
	we either have input data or final isn't set.  We only set
	final when we have used all input data and flushed all 
	output data.  So there is no way for us to exit without
	coming back properly.
***/

    (void) GetCurrentSrc(flo,pet,sbnd);
    if (dbnd->final) {
	/* be forgiving if extra data gets passed to us */
  	FreeData(flo,pet,sbnd,sbnd->maxGlobal);
	return(TRUE);
    }
    while ((dst = (BytePixel*)GetDstBytes(flo,pet,dbnd,dbnd->current,
					 state->strip_req_newbytes,FLUSH)) != 0) {
      if (state->flush_output) {

	status = FlushJpegEncodeData(dbnd,dst,state);
	    /* write as much as we can to output strip. This 	*/
	    /* also updates state->i_line and state->nl_coded. 	*/

	if (status == JPEG_FLUSH_FlushedAll) {
	   state->strip_req_newbytes = dbnd->maxLocal - dbnd->current;
	   state->cinfo->bytes_in_buffer = 0;
	   if (state->goal == JPEG_ENCODE_GOAL_Done) {
	      /* we're done and nothing to flush, so let's wrap up */
	      SetBandFinal(dbnd);
	      PutData(flo,pet,dbnd,dbnd->maxGlobal);
  	      FreeData(flo,pet,sbnd,sbnd->maxGlobal);
	      if (sbnd1) {
  	        FreeData(flo,pet,sbnd1,sbnd1->maxGlobal);
  	        FreeData(flo,pet,sbnd2,sbnd2->maxGlobal);
	      }
	      return(TRUE);
	   }
	   continue;
		/* go around loop again. Get another dst */
		/* (if needed) and encode some more	 */
	}
	else if (status == JPEG_FLUSH_FlushedPart) {
	    /* go get another Destination strip */
  	    state->strip_req_newbytes = flo->floTex->stripSize;
	    PutData(flo,pet,dbnd,dbnd->current);
	    continue;
	}
	else 
      	    ImplementationError(flo,ped, return(FALSE));

      } /* end of if flush */
      else {
	state->nl_tocode  = 0;
	if (state->i_line < state->height) {

	    /* haven't reached the end of the image yet */ 
  	    int nl_mappable = sbnd->maxLocal - sbnd->current;

	    if (sbnd->current != state->i_line)
		OperatorError(flo,ped,123,return(FALSE)) ;

  	    if (!MapData(flo,pet,sbnd,0,sbnd->current,nl_mappable,FLUSH)) {
    		FreeData(flo,pet,sbnd,sbnd->maxLocal);
		if (sbnd1) {
    		  FreeData(flo,pet,sbnd1,sbnd1->maxLocal);
    		  FreeData(flo,pet,sbnd2,sbnd2->maxLocal);
		}
		return(TRUE);	/* need another input strip */
	    }

	    state->i_lines[0] = (unsigned char **)sbnd->dataMap;
	    state->nl_tocode  = sbnd->maxLocal - sbnd->current;

	    if (sbnd1) {

  	      if (!MapData(flo,pet,sbnd1,0,sbnd1->current,nl_mappable,FLUSH)) {
    		FreeData(flo,pet,sbnd,sbnd->maxLocal);
    		FreeData(flo,pet,sbnd1,sbnd1->maxLocal);
    		FreeData(flo,pet,sbnd2,sbnd2->maxLocal);
		return(TRUE);	/* need another input strip */
	      }
	      state->i_lines[1] = (unsigned char **)sbnd1->dataMap;

  	      if (!MapData(flo,pet,sbnd2,0,sbnd2->current,nl_mappable,FLUSH)) {
    		FreeData(flo,pet,sbnd,sbnd->maxLocal);
    		FreeData(flo,pet,sbnd1,sbnd1->maxLocal);
    		FreeData(flo,pet,sbnd2,sbnd2->maxLocal);
		return(TRUE);	/* need another input strip */
	      }
	      state->i_lines[2] = (unsigned char **)sbnd2->dataMap;
	   }
	}  /* end of if (state->i_line < state->height) */

	if (state->cinfo->bytes_in_buffer) {
	    /* this should not be possible if the flushing code is working */
      	    ImplementationError(flo,ped, return(FALSE));
	}
	if ( (*(texpvt->encodptr))(state) < 0 ) 
      	    ImplementationError(flo,ped, return(FALSE));
		/* coding error.  We should be able to encode anything! */
	
	sbnd->current = state->i_line;
	if (sbnd1) 
		sbnd1->current = sbnd2->current = state->i_line;

	if (state->flush_output) {  
	    /* encoder wants us to flush its buffer before we call  */
	    /* it again. record position of where to start flushing */

	    state->jpeg_output_bpos = (unsigned char *) 
		state->cinfo->output_buffer;
	    continue;
	}
	if (state->goal == JPEG_ENCODE_GOAL_Done) {
	    /* we're done and nothing to flush, so let's wrap up */
	    SetBandFinal(dbnd);
	    PutData(flo,pet,dbnd,dbnd->maxGlobal);
  	    FreeData(flo,pet,sbnd,sbnd->maxGlobal);
	    if (sbnd1) {
  	      FreeData(flo,pet,sbnd1,sbnd1->maxGlobal);
  	      FreeData(flo,pet,sbnd2,sbnd2->maxGlobal);
	    }
	    return(TRUE);
	}
	if (!state->nl_tocode) {
	    /* we have no input left */
	    if (state->i_line < state->height) {
		/* need another input strip */
    		FreeData(flo,pet,sbnd,sbnd->current);
		if (sbnd1) {
    		  FreeData(flo,pet,sbnd1,sbnd1->current);
    		  FreeData(flo,pet,sbnd2,sbnd2->current);
		}
		return(TRUE);	
	    }
	    else
		state->goal = JPEG_ENCODE_GOAL_EndFrame;
	}
	 
      } /* end of else !flush */

    } /* end of while (GetDstBytes) */

  return(TRUE);
}                               /* end ActivateEPhotoJPEGBaseline */
/*------------------------------------------------------------------------
-------------------  flush JPEG buffer to output strip -------------------
------------------------------------------------------------------------*/
static int FlushJpegEncodeData(
     bandPtr           dbnd,
     register unsigned char *dst,
     JpegEncodeState *state)
{
register unsigned char *jpeg_odata = state->jpeg_output_bpos;
int bytes_left_in_strip;
register int i,max_can_do;

   bytes_left_in_strip = dbnd->maxLocal - dbnd->current;
	/* dbnd->current is the offset from start of strip, */
	/* so it represents the number of bytes used	    */
   max_can_do  = (bytes_left_in_strip > state->flush_output)? 	
			state->flush_output : bytes_left_in_strip;

   for (i=0; i<max_can_do; ++i)
	*dst++ = *jpeg_odata++;

   dbnd->current 		+= max_can_do;
   state->flush_output 		-= max_can_do;
   state->jpeg_output_bpos 	+= max_can_do;

   if (state->flush_output)
	return(JPEG_FLUSH_FlushedPart);
   else
	return(JPEG_FLUSH_FlushedAll);
}
/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
int ResetEPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped)
{
  ResetReceptors(ped);
  ResetEmitter(ped);
  
  /* get rid of the any data malloc'd by JPEG encoder
   */
  if(ped->peTex) {
    jpegPvtPtr texpvt = (jpegPvtPtr) ped->peTex->private;
    int b;
    
    /* JPEG code has its own global free routine
     */
    for (b=0; b<texpvt->in_bands; ++b)  {
      if(texpvt->cinfo[b].emethods && texpvt->cinfo[b].emethods->c_free_all)
	(*texpvt->cinfo[b].emethods->c_free_all)(& texpvt->cinfo[b]);
    }
  }
  return(TRUE);
}                               /* end ResetEPhotoJPEGBaseline */

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
int DestroyEPhotoJPEGBaseline(floDefPtr flo, peDefPtr ped)
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
}                               /* end DestroyEPhotoJPEGBaseline */
/* end module mejpeg.c */
