/* $Xorg: mifax.c,v 1.4 2001/02/09 02:04:27 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module mifax.c ****/
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
  
	mifax.c --    DDXIE prototype import client photo and 
			import photomap element, portions specific 
			to FAX decompression
  
	Ben Fahy -- AGE Logic, Inc. July, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/import/mifax.c,v 3.5 2001/12/14 19:58:27 dawes Exp $ */

#define _XIEC_MICPHOTO
#define _XIEC_ICPHOTO
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
#include <error.h>
#include <macro.h>
#include <photomap.h>
#include <element.h>
#include <texstr.h>
#include <memory.h>
#include <fax.h>
/* XXX - this should be cleaned up. */
#include "../fax/faxint.h"

/*
 *  routines referenced by other DDXIE modules
 */
int CreateICPhotoFax();
int InitializeICPhotoFax();
int InitializeIPhotoFax();
int ActivateICPhotoFax();
int ResetICPhotoFax();
int DestroyICPhotoFax();


extern bandMsk miImportStream();

/*
 * Local Declarations
 */

static int common_init();


#define LENIENCY_LIMIT  16

#define MAX_STRIPS_INC	20
typedef struct _faxpvt {
  FaxState state;
  int width,height;	/* not strictly necessary, but handy	*/
  int max_strips;	/* how many we've allocated space for	*/
  int n_strips;		/* how many strips we've seen so far 	*/
  int next_byte;	/* next input byte we are looking for	*/
  int max_lines;	/* maximum number of lines/output strip	*/
  char **o_lines;	/* array of pointers to output lines	*/
  int 	(*decodptr)();	/* function used to decode the data	*/
  xieTypOrientation encodedOrder;
  int notify;		/* (ICP) we might have to send an event	*/
  int normal;		/* if not, swap the output bit order	*/
   photomapPtr map;	/* (IP)  just in case we need it	*/
  unsigned char *buf;	/* in case we have to bit reverse src	*/
  int bufsize;		/* buffer size has to vary with input   */
			/* strip size, alas and alak.  :-(	*/
  xieTypDecodeTechnique technique;
} faxPvtRec, *faxPvtPtr;

/*------------------------------------------------------------------------
---------------------------- create peTex . . . --------------------------
------------------------------------------------------------------------*/
int CreateICPhotoFax(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* attach an execution context to the photo element definition */
  return MakePETex(flo,ped, sizeof(faxPvtRec), NO_SYNC, NO_SYNC); 
}                               /* end CreateICPhotoFax */

/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
int InitializeIPhotoFax(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
photomapPtr map = ((iPhotoDefPtr)ped->elemPvt)->map;
peTexPtr pet = ped->peTex;
faxPvtPtr texpvt=(faxPvtPtr) pet->private;
bandPtr     sbnd = &pet->receptor[IMPORT].band[0];

 	if (!common_init(flo,ped,(pointer)map->tecParms,map->technique))
		return(0);

 	texpvt->map = map;	/* can be used as flag in Activate routine */ 

 	return( 
	  ImportStrips(flo,pet,&pet->receptor[0].band[0],&map->strips[0])
	); 
}					/* end InitializeIPhotoFax */
/*------------------------------------------------------------------------
---------------------------- initialize peTex . . . ----------------------
------------------------------------------------------------------------*/
int InitializeICPhotoFax(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
xieFloImportClientPhoto *raw = (xieFloImportClientPhoto *) ped->elemRaw;
peTexPtr		 pet = ped->peTex;
faxPvtPtr	      texpvt = (faxPvtPtr) pet->private;

	if( !common_init(flo,ped,(pointer) &raw[1],raw->decodeTechnique) )
		return(0);

	texpvt->notify = raw->notify;
	return(1);
}					/* end InitializeICPhotoFax */
/*------------------------------------------------------------------------
------- lots of stuff shared between ICPhoto and IPhoto. . . -------------
------------------------------------------------------------------------*/
static int common_init(flo,ped,tec,technique)
floDefPtr flo;
peDefPtr  ped;
pointer tec;		/* we won't know the type until later */
xieTypDecodeTechnique technique;
			/*  (but we can get it from this  :-) */
{
peTexPtr     pet = ped->peTex;
faxPvtPtr texpvt = (faxPvtPtr) pet->private;
formatPtr    inf = pet->receptor[IMPORT].band[0].format;
int	  pbytes;

/* Start over with clean structure */
  bzero(texpvt,sizeof(faxPvtRec));


/* Now start adding what we know */
  texpvt->width  	= inf->width;
  texpvt->height 	= inf->height;
  texpvt->state.width   = inf->width;
	/* nice thing about using receptor format is that it doesn't matter */
	/* whether you are importing from a client or a photomap	*/

  switch(texpvt->technique = technique) {
  case xieValDecodeG31D: 
	{
	xieTecDecodeG31D *tecG31D=(xieTecDecodeG31D *)    tec;
		texpvt->decodptr 	  = decode_g31d;   
		texpvt->encodedOrder 	  = tecG31D->encodedOrder;
  		texpvt->state.goal     	  = FAX_GOAL_SkipPastAnyToEOL;
  		texpvt->state.radiometric = tecG31D->radiometric;
		texpvt->normal 		  = tecG31D->normal;
	}
	break;
  case xieValDecodeG32D:
	{
	xieTecDecodeG32D *tecG32D=(xieTecDecodeG32D *)    tec;
		texpvt->decodptr 	  = decode_g32d;   
		texpvt->encodedOrder 	  = tecG32D->encodedOrder;
  		texpvt->state.goal     	  = FAX_GOAL_SeekEOLandTag;
  		texpvt->state.radiometric = tecG32D->radiometric;
		texpvt->normal 		  = tecG32D->normal;
	}
	break;
  case xieValDecodeG42D: 
	{
	xieTecDecodeG42D *tecG42D=(xieTecDecodeG42D *)    tec;
		texpvt->decodptr 	  = decode_g4;   
		texpvt->encodedOrder 	  = tecG42D->encodedOrder;
  		texpvt->state.goal     	  = FAX_GOAL_StartNewLine;
  		texpvt->state.radiometric = tecG42D->radiometric;
		texpvt->normal 		  = tecG42D->normal;
	}
	break;
  case xieValDecodeTIFF2: 
	{
	xieTecDecodeTIFF2 *tecTIFF2=(xieTecDecodeTIFF2 *) tec;
		texpvt->decodptr 	  = decode_tiff2;   
		texpvt->encodedOrder 	  = tecTIFF2->encodedOrder;
		texpvt->state.goal     	  = FAX_GOAL_StartNewLine;
		texpvt->state.radiometric = tecTIFF2->radiometric;
		texpvt->normal 		  = tecTIFF2->normal;
	}
	break;
  case xieValDecodeTIFFPackBits: 
	{
	xieTecDecodeTIFFPackBits *tecTIFFPackBits=
	   (xieTecDecodeTIFFPackBits *)tec;
		texpvt->decodptr 	  = decode_tiffpb;   
		texpvt->encodedOrder 	  = tecTIFFPackBits->encodedOrder;
		texpvt->state.goal     	  = PB_GOAL_StartNewLine;
		texpvt->normal 		  = tecTIFFPackBits->normal;
	}
	break;
  }
  texpvt->state.a0_color = WHITE;
  texpvt->state.a0_pos   = (-1);

/* things that are zero don't really need to be initialized - consider
 * these comments.
 */
  texpvt->state.magic_needs = 0;
  texpvt->state.strip_state = StripStateNone;
  texpvt->state.strip	    = 0;
  texpvt->state.strip_size  = 0;
  texpvt->state.final  	    = 0;
  texpvt->state.o_line	    = 0;

/*
 * Things that do need to be initialized
 */
  texpvt->state.write_data = 1;
  texpvt->max_strips = MAX_STRIPS_INC;


/* the following must be freed explicitly */

  texpvt->state.old_trans = (int *) XieMalloc(texpvt->width * sizeof(int));
  if (!texpvt->state.old_trans) AllocError(flo,ped, return(FALSE));

  texpvt->state.new_trans = (int *) XieMalloc(texpvt->width * sizeof(int));
  if (!texpvt->state.new_trans) AllocError(flo,ped, return(FALSE));

/* 
 * I suppose this will make Bob mad at me, but... how else do I
 * figure out the output map size? 
 */

  pbytes = (ped->outFlo.format[0].pitch + 7) >> 3;

  texpvt->max_lines = flo->floTex->stripSize / pbytes;

  if (!texpvt->max_lines)
	texpvt->max_lines++;

/* see if data manager should forward our input data to downstream elements */
  pet->receptor[IMPORT].forward = miImportStream(flo,ped);

/* set output emitter to map texpvt->max_lines lines of data */
  return( InitReceptors(flo, ped, NO_DATAMAP, 1) && 
	  InitEmitter(flo, ped, texpvt->max_lines, NO_INPLACE) );

}        					/* end common_init() */
/*------------------------------------------------------------------------
----------------------------- crank some data ----------------------------
------------------------------------------------------------------------*/
int ActivateICPhotoFax(flo,ped,pet)
     floDefPtr flo;
     peDefPtr  ped;
     peTexPtr  pet;
{
  bandPtr     sbnd = &pet->receptor[IMPORT].band[0];
  bandPtr     dbnd = &pet->emitter[0];
  faxPvtPtr texpvt = (faxPvtPtr) ped->peTex->private;
  FaxState  *state = &(texpvt->state);
  BytePixel *src, *dst;
  int	    lines_found;
  Bool ok, aborted;
  int 	(*decodptr)() = texpvt->decodptr;

  
/*
 *  get current input and output strips
 */
  if (dbnd->final  && dbnd->current >= dbnd->format->height) {
	/* be forgiving if extra data gets passed to us */
  	FreeData(flo,pet,sbnd,sbnd->maxGlobal);
	return(TRUE);
  }

 /* 
  * Important!  As long as there is source data available, we'd better
  * deal with it, because it may be owned by Core X and will die if we 
  * let the scheduler go away.  It's ok to exit with TRUE to let an 
  * output strip be written (we'll be called back). It is death to
  * exit with FALSE unless we are absolutely sure we are done with all
  * current input strips.
  */

  /*  
   * Note that state->strip holds the current src strip.  We only get
   * more bytes when we are done with a strip and need another one
   * (or if we are just starting)
   */
  if (!state->strip) {
    src = (BytePixel*)GetSrcBytes(flo,pet,sbnd,sbnd->current,1,KEEP);

    state->strip_state = StripStateNew;
    state->strip_size  = sbnd->maxLocal - sbnd->minLocal;
    state->final       = sbnd->strip ? sbnd->strip->final : sbnd->final;

    if (texpvt->encodedOrder == xieValMSFirst) 
      state->strip = src;
    else {
      /* to avoid rewriting a zillion macros,  we arbitrarily pick MSFirst 
	 as the encode/decode format.  Byte Reversal of the small amount 
	 of compressed data is almost always going to take trivial time.
	 The only nasty part is, the input strip cannot be corrupted so
	 we have to copy to a buffer,  whose size may be variable since
	 this is stream data (not canonical).
      */
      register int i,size=state->strip_size;
      register unsigned char *flipped;
      
      if ( AlterSrc(flo,ped,sbnd->strip) ) 
	 flipped = src;
      else {
	/* make sure buffer is big enough to hold data */
	if (!texpvt->buf) {
	    texpvt->buf = (unsigned char *) XieMalloc(size);
	    if (!texpvt->buf)
  		AllocError(flo,ped, return(FALSE));
	}
	else if (texpvt->bufsize < size) {
	    texpvt->buf = (unsigned char *) XieRealloc(texpvt->buf,size);
	    if (!texpvt->buf)
  		AllocError(flo,ped, return(FALSE));
	}
	texpvt->bufsize = size;
	flipped = texpvt->buf;
      }
      
      for (i=0; i<size; ++i) 
	   flipped[i] = _ByteReverseTable[*src++];
         
      state->strip = flipped;
    }
  }
  /*
   * We have some data to decode, anything to write to?
   */
  while (dst = (BytePixel*)GetDst(flo,pet,dbnd,state->o_line,KEEP)) {
    
    
    /*
     *  Now, as much as I'd like to use our nifty line-oriented macros,
     *  it would kill performance.  So instead we ask the decoder to 
     *  decode as much as remains in the output line as it can get.
     *  We ask the data manager to map all the lines of data in the 
     *  current output strip to a convenient array.
     */
    
    state->nl_sought = dbnd->maxLocal - state->o_line; 
    
    ok = MapData(flo,pet,dbnd,0,dbnd->current,state->nl_sought,KEEP);
    /* map desired lines into an array starting at 0 */
    if (!ok)
      ImplementationError(flo,ped, return(FALSE));
    
    state->o_lines = (char **) dbnd->dataMap;
    
    lines_found = (*decodptr)(state);
    if (lines_found < 0) {
      /* decoder hit unknown error. Pass error code to client */
      ValueError(flo,ped,(state->o_line + (state->decoder_done << 16)), 
		 return(FALSE));
    } else {
      if (!texpvt->normal) {
	/* have to swap bit order on output */
	int pbytes = (ped->outFlo.format[0].pitch + 7) >> 3;
	register int i;
	for (i=0; i<lines_found; ++i) {
	  register CARD8 *ucp = dbnd->dataMap[i];
	  register int size=pbytes;
	  while (size--)	
	  {
	    *ucp = _ByteReverseTable[*ucp];
	    ucp++;
	  }
	}
      }
      state->o_line += lines_found;
      if (PutData(flo,pet,dbnd,state->o_line))
	return(TRUE);
    }
    if (state->decoder_done) {
      /* Decoders sometimes return errors near the end of an image.
	 Don't report an error if we got almost all the lines we wanted.
       */
      if (state->decoder_done > FAX_DECODE_DONE_OK) {
	  aborted = state->o_line + LENIENCY_LIMIT < texpvt->height;

	  if(texpvt->notify && (texpvt->height != state->o_line ||
				texpvt->width  != state->width)) {
	      SendDecodeNotifyEvent(flo, ped, 0, texpvt->technique,
				    state->width, state->o_line, aborted);
	  }
          if (aborted) {
	      ValueError(flo, ped, (state->o_line + (state->decoder_done<<16)),
			 return(FALSE));
	  } else if(state->o_line < texpvt->height) {
	      int bytes = (ped->outFlo.format[0].pitch + 7) >> 3;

	      while(dst = ((BytePixel*)
			   GetDst(flo,pet,dbnd,state->o_line++,KEEP))) {
		  memset(dst, state->radiometric ? 0xff: 0, bytes);
		  PutData(flo,pet,dbnd,state->o_line);
	      }
	      break;
	  }
      }
      FreeData(flo,pet,sbnd,sbnd->maxGlobal);
      break;
    }
    
    if (state->magic_needs) {
      /* decoder needs a new strip */
      if (state->strip_state != StripStateDone)  {
	ImplementationError(flo,ped, return(FALSE));
      }
      FreeData(flo,pet,sbnd,sbnd->maxLocal);
      
      texpvt->state.strip_state = StripStateNone;
      texpvt->state.strip	= 0;
      texpvt->state.strip_size  = 0;
      
      if (!state->final) 
	break;
      else
	state->strip_state = StripStateNoMore;
      /* need to live with what you've got */
    }
  } /* end of while(dst = ...) */
  /*
   *  No more dst or need another src strip? three possible reasons:
   *
   *  1:  we're done with output image.  Just shut down and be happy
   *  2:  scheduler has noticed a downstream element can run. In this
   *      case, we should be a good guy and return TRUE so the scheduler
   *      gives us back control again later (without going back to Core
   *	     X, which would cause our input strip to vanish).
   *  3:  we ran out of src.
   */
  if (!dst && dbnd->final) {
    FreeData(flo,pet,sbnd,sbnd->maxGlobal);
  }
  SetBandThreshold(sbnd, !dst ? 1 : sbnd->available + 1);
  
  /* 
   * if here, we ran out of src.  Scheduler will wake us up again
   * when a PutClientData request comes along.
   */
  return TRUE;
}                               /* end ActivateICPhotoFax */

/*------------------------------------------------------------------------
------------------------ get rid of run-time stuff -----------------------
------------------------------------------------------------------------*/
int ResetICPhotoFax(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  ResetReceptors(ped);
  ResetEmitter(ped);

  /* get rid of the peTex structures malloc'd by Initialize  */
  if(ped->peTex) {
     faxPvtPtr texpvt = (faxPvtPtr) ped->peTex->private;

    /* only have to nuke parts of private structure which were malloc'd */
    if (texpvt->state.old_trans)
	texpvt->state.old_trans = (int *)XieFree(texpvt->state.old_trans);
    if (texpvt->state.new_trans)
	texpvt->state.new_trans = (int *)XieFree(texpvt->state.new_trans);
    if (texpvt->buf)
	texpvt->buf = (unsigned char *)XieFree(texpvt->buf);
  }
  return(TRUE);
}                               /* end ResetICPhotoFax */

/*------------------------------------------------------------------------
-------------------------- get rid of this element -----------------------
------------------------------------------------------------------------*/
int DestroyICPhotoFax(flo,ped)
     floDefPtr flo;
     peDefPtr  ped;
{
  /* get rid of the peTex structure  */
  if(ped->peTex) {
    ped->peTex = (peTexPtr) XieFree(ped->peTex);
  }

  /* zap this element's entry point vector */
  ped->ddVec.create     = (xieIntProc) NULL;
  ped->ddVec.initialize = (xieIntProc) NULL;
  ped->ddVec.activate   = (xieIntProc) NULL;
  ped->ddVec.flush      = (xieIntProc) NULL;
  ped->ddVec.reset      = (xieIntProc) NULL;
  ped->ddVec.destroy    = (xieIntProc) NULL;

  return(TRUE);
}                               /* end DestroyICPhoto */

/* end module mifax.c */
