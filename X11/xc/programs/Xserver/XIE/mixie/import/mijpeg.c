/* $Xorg: mijpeg.c,v 1.4 2001/02/09 02:04:27 xorgcvs Exp $ */
/**** module mijpeg.c ****/
/******************************************************************************
Copyright 1993, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.


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

	mijpeg.c --    DDXIE import photomap element, portions 
			specific to JPEG decompression

	Ben Fahy -- AGE Logic, Inc. Oct, 1993

*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/import/mijpeg.c,v 3.7 2001/12/14 19:58:28 dawes Exp $ */

#define _XIEC_MIPHOTO
#define _XIEC_IPHOTO

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

#ifndef XIE
#define XIE
#endif

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
int CreateIPhotoJpegBase();
int InitializeIPhotoJpegBase();
int InitializeICPhotoJpegBase();
int ActivateIPhotoJpegBase();
int ResetIPhotoJpegBase();
int DestroyIPhotoJpegBase();


extern bandMsk miImportStream();

/*
 * Local Declarations
 */

static int common_init();
static int sub_fun();
typedef struct _jpeg_decode_pvt {

  int 	(*decodptr)();		/* function used to decode the data	*/
  int 	format_checked;		/* have to wait until JPEG header read	*/

  xieTypDataClass dataClass;	/* SingleBand or TripleBand		*/
  int	colors_smushed;		/* if color and interleave = False	*/
  int	in_bands;		/* bands of input (from photomap) data	*/
  int	out_bands;		/* bands of output data			*/
  int	swizzle;		/* true if bandOrder is MSFirst		*/
  int	notify;			/* in case the client wants an event	*/
  photomapPtr map;		/* for ImportPhotomap only		*/

  xieTecDecodeJPEGBaseline *decodeParams;   /*   decoding parameters	*/ 
  
  /* the following are used by JPEG private routines	*/
  struct Decompress_methods_struct dc_methods;
  struct External_methods_struct e_methods;

  /* we potentially need one of the following per band */
  JpegDecodeState state[3];
  struct Decompress_info_struct cinfo[3];
  char   ibuffer[3][JPEG_BUF_SIZE + MIN_UNGET];

} jpegPvtRec, *jpegPvtPtr;


/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
int CreateIPhotoJpegBase(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* attach an execution context to the photo element definition */
  return MakePETex(flo,ped, sizeof(jpegPvtRec), NO_SYNC, NO_SYNC);
}                               /* end CreateIPhotoJpegBase */

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
int InitializeIPhotoJpegBase(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  photomapPtr map = ((iPhotoDefPtr)ped->elemPvt)->map;
  xieTecDecodeJPEGBaseline *tec = (xieTecDecodeJPEGBaseline *) map->tecParms;
  xieFloImportPhotomap     *raw = (xieFloImportPhotomap *)ped->elemRaw;
  int 	   in_bands = ped->inFloLst[SRCtag].bands; /* # of  input bands */
  peTexPtr      pet = ped->peTex;
  jpegPvtPtr texpvt = (jpegPvtPtr) pet->private;
  bandPtr       bnd = pet->receptor[IMPORT].band;

  if(!common_init(flo,ped,map->dataClass,tec,raw->notify))
    return(FALSE);
  
  texpvt->map = map;
  /* has to come after common_init, which bzeros texpvt */
  
  return(ImportStrips(flo, pet, &bnd[0], &map->strips[0]) &&
	(in_bands == 1 ? TRUE :
	 ImportStrips(flo, pet, &bnd[1], &map->strips[1]) &&
	 ImportStrips(flo, pet, &bnd[2], &map->strips[2])));
}					/* end InitializeIPhotoJpegBase */

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
int InitializeICPhotoJpegBase(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
xieFloImportClientPhoto *raw = (xieFloImportClientPhoto *)ped->elemRaw;
xieTecDecodeJPEGBaseline *tec= (xieTecDecodeJPEGBaseline*)&raw[1];

	return (common_init(flo,ped,raw->class,tec,raw->notify) );

} 					/* end InitializeICPhotoJpegBase */
/*------------------------------------------------------------------------
----------------------- shared initialization code  ----------------------
------------------------------------------------------------------------*/
static int common_init(flo,ped,class,tec,notify)
     floDefPtr flo;
     peDefPtr  ped;
     xieTypDataClass class;
     xieTecDecodeJPEGBaseline *tec;
     int notify;
{
  peTexPtr      pet = ped->peTex;
  jpegPvtPtr texpvt = (jpegPvtPtr) pet->private;
  int        pbytes;
  int     out_bands = ped->outFlo.bands;	    /* # of output bands */
  int      in_bands = ped->inFloLst[SRCtag].bands;  /* # of  input bands */
  int 	    lines_in_output_strip;    /* max number of lines/output strip*/
  int 	    b;
  
  /*** every time we rerun the element, have to reset it ***/
  
  bzero(texpvt,sizeof(jpegPvtRec));
  /* this does 99% of what we want */
  
  /*** do generic stuff first ***/
  texpvt->in_bands     = in_bands;
  texpvt->out_bands    = out_bands;
  texpvt->dataClass    = class; 
  texpvt->decodeParams = tec;
  texpvt->notify       = notify;
  
  if(in_bands != out_bands) {	   /* ie, if photo has 1 band, output 3 */
    texpvt->colors_smushed = 1;
    texpvt->decodptr = decode_jpeg_lossy_color;
  } else {			   /* SingleBand or TripleBand-BandByPlane */
    texpvt->colors_smushed = 0;
    texpvt->decodptr = decode_jpeg_lossy_gray;
  }
  texpvt->swizzle = tec->bandOrder == xieValMSFirst;
  
  /* Now for every input (photomap) band, we need to set up state stuff */
  for (b=0; b < in_bands; ++b) {
    JpegDecodeState *state = &(texpvt->state[b]);
    
    state->goal       = JPEG_DECODE_GOAL_Startup;
    state->up_sample  = tec->upSample;
    state->dc_methods = &texpvt->dc_methods;
    state->e_methods  = &texpvt->e_methods;
    state->cinfo      = &texpvt->cinfo[b];
    state->needs_input_strip   = 1;
    state->cinfo->input_buffer = texpvt->ibuffer[b];
  }
  
  /*** Note: the JPEG code allocates an additional buffer after the
    header data is read. The size of the buffer depends on
    the image dimenstions, so this needs to be deallocated
    in the Reset entry point.  The rest of the above is of
    fixed size, and will disappear automatically at Destroy
    ***/
  
  /* Calculate how many lines fit in an output strip
   */
  pbytes = (ped->outFlo.format[0].pitch + 7) >> 3;
  lines_in_output_strip = flo->floTex->stripSize / pbytes;
  
  if (!lines_in_output_strip)
    lines_in_output_strip++;
  
  /* see if data manager should forward our input data to downstream elements
   */
  pet->receptor[IMPORT].forward = miImportStream(flo,ped);

  /* set output emitter to allocate a map big enough to hold whole strip
   */
  return(InitReceptors(flo, ped, NO_DATAMAP, 1) &&
	 InitEmitter(flo, ped, lines_in_output_strip, NO_INPLACE));
  
}                               /* end InitializeIPhotoJpegBase */

/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
int ActivateIPhotoJpegBase(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  jpegPvtPtr texpvt = (jpegPvtPtr) ped->peTex->private;
  receptorPtr   rcp =  pet->receptor;
  bandPtr      sbnd = &pet->receptor[IMPORT].band[0];
  bandPtr     dbnd  = &pet->emitter[0];
  bandPtr     dbnd1 = &pet->emitter[1];
  bandPtr     dbnd2 = &pet->emitter[2];
  int b;
  
  /* dbnd and dst are used to deal with non-interleaved color data	*/
  /* if the class of the src is SingleBand, we call sub_fun for band 0 	*/
  /* if the class of the src is TripleBand and the interleave is 	*/
  /* 	BandByPlane, we call sub_fun 3 times, once for each band	*/
  /* if the class of the src is TripleBand, interleave BandByPixel, we	*/
  /* 	call subfun once:  it will produce all three dst bands		*/
  
  if (texpvt->dataClass == xieValSingleBand) {
    JpegDecodeState *state = &(texpvt->state[0]);
    return( sub_fun(flo,ped,pet,texpvt,state,sbnd,dbnd,NULL,NULL) );
  }
  
  /* TripleBand */
  if (texpvt->colors_smushed) {				/* BandByPixel */
    JpegDecodeState *state = &(texpvt->state[0]);
    if(texpvt->swizzle)
      return( sub_fun(flo,ped,pet,texpvt,state,sbnd,dbnd2,dbnd1,dbnd) );
    else
      return( sub_fun(flo,ped,pet,texpvt,state,sbnd,dbnd,dbnd1,dbnd2) );
  }
  
  /* TripleBand, BandByPlane */
  for(b = 0; b < xieValMaxBands; ++b) {
    JpegDecodeState *state = &(texpvt->state[b]);
    sbnd = &rcp->band[b];	/* do each band independently */
    dbnd = &pet->emitter[texpvt->swizzle ? xieValMaxBands - b - 1 : b];
    if ( sub_fun(flo,ped,pet,texpvt,state,sbnd,dbnd,NULL,NULL)==FALSE)
      return(FALSE);
  }
  return(TRUE);
}

/*------------------------------------------------------------------------
-------------------- *really* crank some data ----------------------------
------------------------------------------------------------------------*/
static int sub_fun(flo,ped,pet,texpvt,state,sbnd,dbnd,dbnd1,dbnd2)
 floDefPtr flo;
 peDefPtr  ped;
 peTexPtr  pet;
 jpegPvtPtr texpvt;
 JpegDecodeState *state;
 bandPtr     sbnd,dbnd,dbnd1,dbnd2;
{
BytePixel *src, *dst,*dst1,*dst2;
int 	(*decodptr)() = texpvt->decodptr;

/*
 *  get current input and output strips
 */
  if (dbnd->final  && dbnd->current >= dbnd->format->height) {
	/* be forgiving if extra data gets passed to us */
  	FreeData(flo,pet,sbnd,sbnd->maxGlobal);
	return(TRUE);
  }

  src=(BytePixel*)GetSrcBytes(flo,pet,sbnd,sbnd->current,sbnd->available,KEEP);

	if ( state->needs_input_strip )  {
	   /* NOTE:  state->needs_input_strip is ONLY set after checking */
	   /* to ensure that state->final != 1.  In other words, we only */
	   /* arrive here when we are sure there will be source data	 */

	   /* we left because we needed more input. The other case    */
	   /* would be if we left to allow downstream elements to run */

	   state->strip = src;
	   state->sptr  = src;
  	   state->strip_size  = sbnd->maxLocal - sbnd->minLocal;

	   state->i_strip++;	/* just for debugging */

    	   state->final = sbnd->strip ? sbnd->strip->final : sbnd->final;
	   state->needs_input_strip = 0;
	}

     /*
      * We have some data to decode, anything to write to?
      */

      while (dst = (BytePixel*)GetDst(flo,pet,dbnd,state->o_line,FLUSH)) {

	/* if tripleband interleaved data, make sure other bands cool	*/
  	if (dbnd1)
	  if (!(dst1=(BytePixel*)GetDst(flo,pet,dbnd1,state->o_line,FLUSH)) ||
      	      !(dst2=(BytePixel*)GetDst(flo,pet,dbnd2,state->o_line,FLUSH)) )
		ImplementationError(flo,ped, return(FALSE));

	/*
	 *  If we reenter this loop after having decoded some data,
	 *  it's important to check to see if we need to flush the
	 *  decoder's output buffer.  It's big trouble to call the
	 *  decoder if its output buffer isn't flushed. nl_found
	 *  indicates how many lines are left in the buffer
	 */

	if (state->nl_found) {
	    JSAMPIMAGE jpeg_odata = state->cinfo->output_workspace;
	    register int ci,row;
	    int max_can_do;
  	    int nl_mappable;
	    unsigned char **o_lines;

	    /* this is possibly the first time we can check to see 	*/
	    /* if the data agrees with the output format		*/
	    /* we don't get to check the JPEG data format until the	*/
	    /* header has been read. The first time some output lines	*/
	    /* are produced,  we make sure all is copasetic... er... ok	*/

	    if (!texpvt->format_checked) {

		 /* XXX - width, height must be same, all bands */
	        if ( dbnd->format->width  != (int)state->cinfo->image_width  )
      	    	     ImplementationError(flo,ped, return(FALSE));

	        if ( dbnd->format->height != (int)state->cinfo->image_height )
      	    	     ImplementationError(flo,ped, return(FALSE));

	        if ( texpvt->dataClass == xieValSingleBand && 
			state->cinfo->num_components != 1 )
      	    	     ImplementationError(flo,ped, return(FALSE));

		/* due to !*(!(! interleave option, checking bands is harder */
		if (
		  (texpvt->dataClass == xieValTripleBand && 
			texpvt->colors_smushed && 
			state->cinfo->num_components != 3)	||
		  (texpvt->dataClass == xieValTripleBand && 
			!texpvt->colors_smushed &&
			state->cinfo->num_components != 1) )
      	    	     ImplementationError(flo,ped, return(FALSE));

		texpvt->format_checked = 1;
	    }

	    /* Map current output strip to an array of lines, current->0 */
  	    nl_mappable = dbnd->maxLocal - state->o_line;
  	    if (!MapData(flo,pet,dbnd,0,dbnd->current,nl_mappable,KEEP))
      	    	ImplementationError(flo,ped, return(FALSE));

	    if (dbnd1) {
	      /* same for other two bands.  Notice that we rely on 	*/
	      /* all three bands being synchronous with each other 	*/

  	      if (!MapData(flo,pet,dbnd1,0,dbnd1->current,nl_mappable,KEEP))
      	    	ImplementationError(flo,ped, return(FALSE));
  	      if (!MapData(flo,pet,dbnd2,0,dbnd2->current,nl_mappable,KEEP))
      	    	ImplementationError(flo,ped, return(FALSE));
	    }

	    /* limit flushing to as much as fits in a strip */
	    max_can_do = (nl_mappable < state->nl_found)?
			  nl_mappable : state->nl_found;


	    /* at this point, we have checked enough to make sure */
	    /* the following data copying stuff is appropriate	  */

  	    o_lines = (unsigned char **) dbnd->dataMap;
	    for (row=0; row<max_can_do; ++row) {
		register unsigned char *o_line = o_lines[row];
		register JSAMPROW jptr = jpeg_odata[0][row+state->nl_flushed];
		register npix = state->cinfo->image_width;
		while (npix--)
		    *o_line++ = (unsigned char) *jptr++;
	    }

	    for (ci=1; ci < state->cinfo->num_components; ++ci) {
  	      o_lines = (unsigned char **) ((ci==1)? dbnd1 : dbnd2)->dataMap;
	      for (row=0; row<max_can_do; ++row) {
		register unsigned char *o_line = o_lines[row];
		register JSAMPROW jptr = jpeg_odata[ci][row+state->nl_flushed];
		register npix = state->cinfo->image_width;
		while (npix--)
		    *o_line++ = (unsigned char) *jptr++;
	      }
	    }

	    state->o_line     += max_can_do;
	    state->nl_found   -= max_can_do;
	    state->nl_flushed += max_can_do;

            dbnd->current    = state->o_line;
	    if (dbnd1) {
                dbnd1->current    = state->o_line;
                dbnd2->current    = state->o_line;
	    }

	    /* if we are done with this output strip, deliver it */
	    if (state->o_line >= (int) dbnd->maxLocal)  {
		/* if PutData returns True, there is someone ready to
		   be scheduled downstream. We should return to yield
		*/
	        if (dbnd1) {
	            PutData(flo,pet,dbnd, (unsigned) state->o_line);
	            PutData(flo,pet,dbnd1,(unsigned) state->o_line);
	            PutData(flo,pet,dbnd2,(unsigned) state->o_line);
	        }
		else
		  if (PutData(flo,pet,dbnd, (unsigned) state->o_line) )
			return(TRUE);
	    }

	    if (state->nl_found)  	/* couldn't flush all of it  */
		continue;		/* need another output strip */


	    /* if here, flushed everything */
	    if (state->goal != JPEG_DECODE_GOAL_Done)
		continue;		/* go around loop again. get */
	    				/* more dst if necessary     */

	    /* all done!! free up all the src data */
  	    FreeData(flo,pet,sbnd,sbnd->maxGlobal);
	    return TRUE;

	} /* end of if (state->nl_found) */

	else {
	    /* shouldn't call decoder if it wants something to  */
	    /* decode but there is nothing available to decode! */
  	    if (state->needs_input_strip)
     		ImplementationError(flo,ped, return(FALSE));

	    if ( (*decodptr)(state) < 0 ) {
		/* if client wanted to be notified, give him the bad news */
		if(texpvt->notify)
	   	    SendDecodeNotifyEvent(flo, ped, dbnd->band,
			xieValDecodeJPEGBaseline,
			dbnd->format->width, dbnd->current, TRUE);

		/* have to send some error. We didn't get the right 	*/
		/* number of lines, so complain about the height	*/
		ValueError(flo,ped,dbnd->format->height, return(FALSE));
	    }

	    if (state->needs_input_strip) {
  	        FreeData(flo,pet,sbnd,sbnd->maxLocal);
		return TRUE;
	    }

	    /* if we have more work to do or need to flush, carry on */
	    if (state->goal != JPEG_DECODE_GOAL_Done || state->nl_found)
		continue;

	    /* all done!! free up all the src data */
  	    FreeData(flo,pet,sbnd,sbnd->maxGlobal);
	    return TRUE;

	}  /* end of else (!state->nl_found) */

      } /* end of while(dst = ...) */

     /*
      *  No more dst?  Then either we are:
      *
      *  1:  done with output image.  Just shut down and be happy
      *  2:  scheduler has noticed a downstream element can run. In this
      *      case, we should be a good guy and return TRUE so the scheduler
      *      gives us back control again later (without going back to Core
      *	     X, which would cause our input strip to vanish).
      */
      if (dbnd->final) {
	/* all done!! free up all the src data */
  	FreeData(flo,pet,sbnd,sbnd->maxGlobal);
	return TRUE;
      }

/*
 * if here, we ran out of src.  Scheduler will wake us up again
 * when a PutClientData request comes along.
 */
  return TRUE;	/* Hmmm. Well, I think I did my part ok... */
}                               /* end ActivateIPhotoJpegBase */

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
int ResetIPhotoJpegBase(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  ResetReceptors(ped);
  ResetEmitter(ped);

  /* get rid of the any data malloc'd by JPEG decoder  */
  if(ped->peTex) {
     jpegPvtPtr texpvt = (jpegPvtPtr) ped->peTex->private;
     int b;

     /*** JPEG code has its own global free routine ***/
     for (b=0; b<texpvt->in_bands; ++b)  {
       if (texpvt->cinfo[b].emethods)
          (*texpvt->cinfo[b].emethods->d_free_all)(& texpvt->cinfo[b]);
     }
  }
  return(TRUE);
}                               /* end ResetIPhotoJpegBase */

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
int DestroyIPhotoJpegBase(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* get rid of the peTex structure  */
  if(ped->peTex) 
    ped->peTex = (peTexPtr) XieFree(ped->peTex);

  /* zap this element's entry point vector */
  ped->ddVec.create     = (xieIntProc) NULL;
  ped->ddVec.initialize = (xieIntProc) NULL;
  ped->ddVec.activate   = (xieIntProc) NULL;
  ped->ddVec.flush      = (xieIntProc) NULL;
  ped->ddVec.reset      = (xieIntProc) NULL;
  ped->ddVec.destroy    = (xieIntProc) NULL;

  return TRUE;
} 			/* end DestroyIPhotoJpegBase */

/**** module mijpeg.c ****/
