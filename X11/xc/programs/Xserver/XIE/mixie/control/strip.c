/* $Xorg: strip.c,v 1.4 2001/02/09 02:04:23 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module strip.c ****/
/*****************************************************************************

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
  
	strip.c -- DDXIE machine independent data flo manager
  
	Robert NC Shelley -- AGE Logic, Inc. April, 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/control/strip.c,v 3.7 2001/12/14 19:58:17 dawes Exp $ */

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
#include <flostr.h>
#include <element.h>
#include <texstr.h>
#include <memory.h>

/* routines exported to DIXIE via the photoflo management vector
 */
static int import_data(
			floDefPtr flo,
			peDefPtr ped,
			CARD8 band,
			CARD8 *data,
			CARD32 len,
			BOOL final);
static int export_data(
			floDefPtr flo,
			peDefPtr ped,
			CARD8 band,
			CARD32 maxLen,
			BOOL term);
static int query_data(
			floDefPtr flo,
			xieTypPhototag **list,
			CARD16 *pending,
			CARD16 *available);

/* routines exported to elements via the data manager's vector
 */
static CARD8	*make_bytes(
			floDefPtr flo,
			peTexPtr pet,
			bandPtr bnd,
			CARD32 contig,
			Bool purge);
static CARD8	*make_lines(
			floDefPtr flo,
			peTexPtr pet,
			bandPtr bnd,
			Bool purge);
static Bool	 map_data(
			floDefPtr flo,
			peTexPtr pet,
			bandPtr bnd,
			CARD32 map,
			CARD32 unit,
			CARD32 len,
			Bool purge);
static CARD8	*get_data(
			floDefPtr flo,
			peTexPtr pet,
			bandPtr bnd,
			CARD32 contig,
			Bool purge);
static Bool	 put_data(floDefPtr flo, peTexPtr pet, bandPtr dbnd);
static void	 free_data(floDefPtr flo, peTexPtr pet, bandPtr dbnd);
static Bool	 pass_strip(
			floDefPtr flo,
			peTexPtr pet,
			bandPtr bnd,
			stripPtr strip);
static Bool	 import_strips(
			floDefPtr flo,
			peTexPtr pet,
			bandPtr bnd,
			stripLstPtr strips);
static Bool	 alter_src(floDefPtr flo, peTexPtr pet, stripPtr strip);
static void	 bypass_src(floDefPtr flo, peTexPtr pet, bandPtr dbnd);
static void	 disable_src(floDefPtr flo, peTexPtr pet, bandPtr bnd, Bool purge);
static void	 disable_dst(floDefPtr flo, peTexPtr pet, bandPtr dbnd);

/* routines used internal to this module
 */
static stripPtr	 alter_data(floDefPtr flo, peTexPtr pet, bandPtr db);
static stripPtr	 contig_data(
			floDefPtr flo,
			peTexPtr pet,
			bandPtr bnd,
			stripPtr i_strip,
			CARD32 contig);
static stripPtr	 make_strip(
			floDefPtr flo,
			formatPtr fmt,
			CARD32    start,
			CARD32    units,
			CARD32    bytes,
			Bool      allocData);
static stripPtr  clone_strip(floDefPtr flo, stripPtr in_strip);
static bandMsk   put_strip(floDefPtr flo, peTexPtr pet, stripPtr strip);
static void      forward_strip(floDefPtr flo, peTexPtr pet, stripPtr fwd);
static stripPtr  free_strip(floDefPtr flo, stripPtr strip);

/* DDXIE client data manager entry points
 */
static dataVecRec dataManagerVec = {
  (xieIntProc)import_data,
  (xieIntProc)export_data,
  (xieIntProc)query_data
  };

/* DDXIE photoflo manager entry points
 */
static stripVecRec stripManagerVec = {
  make_bytes,
  make_lines,
  map_data,
  get_data,
  put_data,
  free_data,
  pass_strip,
  import_strips,
  alter_src,
  bypass_src,
  disable_src,
  disable_dst
  };

INT32  STRIPS = 0; /* DEBUG */
INT32  BYTES  = 0; /* DEBUG */

/*------------------------------------------------------------------------
-------------------------- Initialize Data Manager -----------------------
------------------------------------------------------------------------*/
int InitStripManager(floDefPtr flo)
{
  /* plug in the DDXIE client data management vector */
  flo->dataVec = &dataManagerVec;

  /* plug in the strip manager vector */
  flo->stripVec = &stripManagerVec;

  /* init the strip cache */
  ListInit(&flo->floTex->stripHead);

  /* choose the best strip size for this flo (a constant value for now) */
  flo->floTex->stripSize = STANDARD_STRIP_SIZE;

  /* clear the count of strips passed */
  flo->floTex->putCnt = 0;

  return(TRUE);
}                               /* end InitStripManager */


/*------------------------------------------------------------------------
----------- discard parent headers from a whole list of strips -----------
-----------   and copy data if multiple references are found   -----------
----------- then transfer them all to their final destination  -----------
------------------------------------------------------------------------*/
int DebriefStrips(
     stripLstPtr i_head,
     stripLstPtr o_head)
{
  stripPtr child, parent;

  /* NOTE: we might want to consider (re)allocing strip buffers to
   *	   strip->length instead of leaving them an strip->bufSiz.
   */
  for(child = i_head->flink; !ListEnd(child,i_head); child = child->flink) {
    while ((parent = child->parent) != 0)
      if(parent->refCnt == 1) {		/* discard a cloned header	  */
	child->parent = parent->parent;
	XieFree(parent);
	--STRIPS; /*DEBUG*/
      } else {				/* copy multiply referenced data  */
	if(!(child->data = (CARD8*)XieMalloc(child->bufSiz)))
	  return(FALSE);
	memcpy((char*)child->data, (char*)parent->data, (int)child->bufSiz);
	child->parent = NULL;		/* de-reference child from parent */
	--parent->refCnt;
	BYTES += child->bufSiz; /*DEBUG*/
      }
    child->format = NULL;		/* kill per-strip format pointer  */
  }
  /* transfer the entire list of input strips to the output
   */
  if(ListEmpty(i_head))
    ListInit(o_head);
  else {
    i_head->flink->blink = (stripPtr)o_head;
    i_head->blink->flink = (stripPtr)o_head;
    *o_head = *i_head;
    ListInit(i_head);
  }
  return(TRUE);
}                               /* end DebriefStrips */


/*------------------------------------------------------------------------
------------------------- Free a whole list of strips --------------------
------------------------------------------------------------------------*/
void  FreeStrips(stripLstPtr head)
{
  while( !ListEmpty(head) ) {
    stripPtr strip;
    
    RemoveMember(strip, head->flink);
    free_strip(NULL, strip);
  }
}                               /* end FreeStrips */


/*------------------------------------------------------------------------
------------------------ Input from PutClientData ------------------------
------------------------------------------------------------------------*/
static int import_data(
     floDefPtr flo,
     peDefPtr  ped,
     CARD8    band,
     CARD8   *data,
     CARD32    len,
     BOOL    final)
{
  peTexPtr    pet =  ped->peTex;
  receptorPtr rcp = &pet->receptor[IMPORT];
  bandPtr     bnd = &rcp->band[band];
  bandMsk     msk = 1<<band;
  stripPtr  strip;

  if(!((rcp->admit | rcp->bypass) & msk))
    return(TRUE);				/* drop unwanted data */

  /* make a strip and fill it in the info from the client
   * (the format info was supplied with the element and technique parameters)
   */
  if(!(strip = make_strip(flo,bnd->format,bnd->maxGlobal,len,len,FALSE)))
    AllocError(flo,ped, return(FALSE));
  strip->final  = final;
  strip->data   = data;
  strip->bufSiz = len;

  if(rcp->bypass & msk) {
    put_strip(flo,pet,strip);			/* pass it downstream */
    if(!strip->flink)
      free_strip(flo,strip);			/* nobody wanted it   */
  } else {
    bnd->maxGlobal  = strip->end + 1;
    bnd->available += len;
    rcp->ready     |= msk;
    InsertMember(strip,bnd->stripLst.blink);
    if ((bnd->final = final) != 0) {
      if(!(rcp->admit &= ~msk))
	--pet->admissionCnt;
      --flo->floTex->imports;
    }
  }
  /* fire up the scheduler -- then we're outa here */
  return( Execute(flo,pet) );
}                               /* end import_data */


/*------------------------------------------------------------------------
-------------------------- Output for GetClientData ----------------------
------------------------------------------------------------------------*/
static int export_data(
     floDefPtr flo,
     peDefPtr  ped,
     CARD8    band,
     CARD32 maxLen,
     BOOL     term)
{
  BOOL       release = FALSE, final = FALSE;
  stripLstPtr    lst = &ped->outFlo.output[band];
  stripPtr     strip = NULL;
  CARD32 bytes, want = maxLen;
  CARD8  state, *data;

  /* if this is multi-byte data, make sure we send/swap complete aggregates
   */
  if(ped->swapUnits[band] > 1)
    want &= ~(ped->swapUnits[band]-1);

  if ((bytes = ListEmpty(lst) ? 0 : min(lst->flink->length, want)) != 0) {
    strip = lst->flink;
    data  = strip->data + (strip->bitOff>>3);
    if(strip->length -= bytes) {
      strip->start   += bytes;
      strip->bitOff  += bytes<<3;  
    } else {
      RemoveMember(strip, strip);
      final   = strip->final;
      release = TRUE;
      if(ListEmpty(lst))
	ped->outFlo.ready &= ~(1<<band);
    }
  } else {
    data = NULL;
  }
  if(final)
    flo->floTex->exports--;
  else if(term) {
    /* shut down the output band prematurely */
    ped->outFlo.ready &= ~(1<<band);
    disable_dst(flo,ped->peTex,&ped->peTex->emitter[band]);
  }
  /* figure out our current state and send a reply to the client
   */
  state = ped->outFlo.ready  & 1<<band ? xieValExportMore
	: ped->outFlo.active & 1<<band ? xieValExportEmpty
	: xieValExportDone;
  SendClientData(flo,ped,data,bytes,ped->swapUnits[band],state);
  if(release)
    free_strip(flo,strip);

  return(bytes ? Execute(flo,NULL) : TRUE);
}                               /* end export_data */


/*------------------------------------------------------------------------
----------- Query flo elements involved in client data transport ---------
------------------------------------------------------------------------*/
static int query_data(
     floDefPtr         flo,
     xieTypPhototag **list,
     CARD16       *pending,
     CARD16     *available)
{
  peDefPtr ped;
  pedLstPtr lst = ListEmpty(&flo->optDAG) ? &flo->defDAG : &flo->optDAG;
  CARD32 exdex;
  
  *pending = *available = 0;
  if(!(*list = (xieTypPhototag *)XieMalloc(flo->peCnt * sz_xieTypPhototag)))
    FloAllocError(flo,0,0, return(FALSE));

  /* find all the import elements that need client data */
  for(ped = lst->flink; ped; ped = ped->clink)
    if(ped->flags.putData && ped->peTex->admissionCnt)
      *list[(*pending)++] = ped->phototag;

  /* find all the export elements that have client data */
  exdex = *pending + (*pending & 1);
  for(ped = lst->blink; ped; ped = ped->clink)
    if(ped->flags.getData && ped->outFlo.ready)
      *list[exdex + (*available)++] = ped->phototag;

  return(TRUE);
}                               /* end query_data */


/*------------------------------------------------------------------------
---- make a strip containing the specified number of contiguous bytes ----
------------------------------------------------------------------------*/
static CARD8* make_bytes(
     floDefPtr	flo,
     peTexPtr	pet,
     bandPtr	bnd,
     CARD32  contig,
     Bool     purge)
{
  stripPtr strip = bnd->stripLst.blink;
  CARD32 limit, size, units;
  Bool avail = (!ListEmpty(&bnd->stripLst) && bnd->current >= strip->start &&
		bnd->current + contig <= strip->start + strip->bufSiz);

  if(purge && !avail && put_data(flo,pet,bnd))
    return(bnd->data = NULL);	    /* force element to suspend processing */
  
  if(_is_global(bnd))
    return(get_data(flo,pet,bnd,contig,FALSE));	 /* "current" is available */

  if(avail) {
    /* extend the available space in our current strip
     */
    limit = bnd->current + contig;
    bnd->available += limit - bnd->maxGlobal;
    bnd->maxGlobal  = limit;
    strip->end      = limit - 1;
    strip->length   = limit - strip->start;
  } else {
    /* time to make a fresh strip
     */
    units = bnd->current + contig - bnd->maxGlobal;
    size  = units + Align(units,flo->floTex->stripSize);
    if(!(strip = make_strip(flo,bnd->format,bnd->maxGlobal,units,size,TRUE)))
      AllocError(flo,pet->peDef, return(NULL));

    bnd->available += strip->length;
    bnd->maxGlobal  = strip->end + 1;
    InsertMember(strip,bnd->stripLst.blink);
  }  
  /* update our bandRec with the results */
  bnd->strip    = strip;
  bnd->minLocal = max(bnd->minGlobal,strip->start);
  bnd->maxLocal = strip->end + 1;
  return(bnd->data = _byte_ptr(bnd));
}                               /* end make_bytes */


/*------------------------------------------------------------------------
--- Find or make a strip containing the unit specified by bnd->current ---
------------------------------------------------------------------------*/
static CARD8* make_lines(
     floDefPtr	flo,
     peTexPtr	pet,
     bandPtr	bnd,
     Bool     purge)
{
  stripPtr strip = NULL;
  formatPtr  fmt;
  CARD32 size, units;
  
  if(purge && _release_ok(bnd) && put_data(flo,pet,bnd))
    return(bnd->data = NULL);	/* force element to suspend processing */
  
  if(_is_global(bnd))	  	/* we already have it, just go find it */
    return(get_data(flo,pet,bnd,(CARD32) 1,FALSE));
  
  fmt = bnd->format;
  if(bnd->current >= fmt->height)
    return(NULL);			/* trying to go beyond end of image */
  
  while(bnd->current >= bnd->maxGlobal) {
    /*
     * re-use src if we're allowed to alter the data
     */
    if(!bnd->inPlace || !(strip = alter_data(flo,pet,bnd))) {
      size  = flo->floTex->stripSize;
      units = size / bnd->pitch;
      if(units == 0) {			/* image bigger than standard strip */
	units = 1;
	size  = bnd->pitch;
      } else if(bnd->current + units > fmt->height)	/* at end of image  */
	units = fmt->height - bnd->current;
      strip = make_strip(flo,fmt,bnd->maxGlobal,units,size,TRUE);
    }
    if(!strip)
      AllocError(flo,pet->peDef, return(NULL));
    bnd->available += strip->length;
    bnd->maxGlobal  = strip->end + 1;
    if(bnd->maxGlobal == fmt->height)
      bnd->final = strip->final = TRUE;
    InsertMember(strip,bnd->stripLst.blink);
  }
  /* update our bandRec with the results */
  bnd->strip    = strip;
  bnd->minLocal = max(bnd->minGlobal,strip->start);
  bnd->maxLocal = strip->end + 1;
  return(bnd->data = _line_ptr(bnd));
}                               /* end make_lines */


/*------------------------------------------------------------------------
-------------- load data map with pointers to specified data -------------
------------------------------------------------------------------------*/
static Bool map_data(
     floDefPtr  flo,
     peTexPtr   pet,
     bandPtr    bnd,
     CARD32     map,
     CARD32     unit,
     CARD32     len,
     Bool	purge)
{
  CARD32 line, pitch;
  CARD8 *next = (CARD8 *)NULL, *last = (CARD8 *)NULL;
  CARD8 **ptr = bnd->dataMap + map;
  stripPtr strip;

  /* first map the last unit and then the first unit -- if we have to make
   * strips, or if input strips aren't there, we may as well handle it now
   */
  if(len && map + len <= bnd->mapSize)
    if(bnd->isInput) {
      last = (CARD8*)GetSrc(flo,pet,bnd,unit+len-1,KEEP);
      next = (CARD8*)GetSrc(flo,pet,bnd,unit,purge);
    } else {
      last = (CARD8*)GetDst(flo,pet,bnd,unit+len-1,KEEP);
      next = (CARD8*)GetDst(flo,pet,bnd,unit,purge);
    }
  if(!next || !last)
    return(FALSE);	/* map too small or can't map first and last unit */

  /* now walk through the strips and map all the lines (or bytes!)
   */
  strip = bnd->strip;
  pitch = bnd->pitch;
  line  = unit;
  while((*ptr++ = next) != last)
    if(++line <= strip->end)
      next += pitch;
    else {
      strip = strip->flink;
      next  = strip->data;
    }
  return(TRUE);
}                               /* end map_data */


/*------------------------------------------------------------------------
------ Find the strip containing the unit specified by bnd->current ------
------------------------------------------------------------------------*/
static CARD8* get_data(
     floDefPtr	flo,
     peTexPtr	pet,
     bandPtr	bnd,
     CARD32  contig,
     Bool     purge)
{
  /* NOTE: get_data assumes that the caller has already verified that the
   *	   beginning of the requested data is available in bnd->stripLst.
   */
  stripPtr strip = bnd->strip ? bnd->strip : bnd->stripLst.flink;
  
  /* first get rid of extra baggage if we can
   */
  if(purge && _release_ok(bnd))
    free_data(flo,pet,bnd);

  strip = bnd->strip ? bnd->strip : bnd->stripLst.flink;
  while(!ListEnd(strip,&bnd->stripLst))
    if(bnd->current > strip->end)
      strip = strip->flink;			/* try the next strip        */
    else if(bnd->current < strip->start)
      strip = strip->blink;			/* try the previous strip    */
    else if(bnd->current+contig-1 <= strip->end ||
	    (strip = contig_data(flo,pet,bnd,strip,contig)))
      break;					/* we found or assembled it  */
    else
      return(NULL);				/* couldn't get enough bytes */

  /* update our bandRec with the results
   */
  bnd->strip    = strip;
  bnd->minLocal = max(bnd->minGlobal,strip->start);
  bnd->maxLocal = strip->end + 1;
  bnd->data     = _is_local_contig(bnd,contig) ? _line_ptr(bnd) : NULL;
  return(bnd->data);
}                               /* end get_data */


/*------------------------------------------------------------------------
------- move strip(s) onto awaiting receptor(s) or an export outFlo ------
------------------------------------------------------------------------*/
static Bool put_data(
     floDefPtr  flo,
     peTexPtr   pet,
     bandPtr    bnd)
{
  bandMsk suspend = 0;
  stripPtr strip;
  
  /* transfer strips until we run out or reach the one we're working in
   */
  while(_release_ok(bnd)) {
    RemoveMember(strip, bnd->stripLst.flink);
    bnd->available -= strip->length;
    bnd->minGlobal  = strip->end + 1;
    
    if(!(pet->emitting & 1<<bnd->band))
      free_strip(flo,strip);			/* output disabled    */
    else {
      strip->flink = NULL;
      suspend |= put_strip(flo,pet,strip);	/* send it downstream */
      if(!strip->flink)
	free_strip(flo,strip);			/* nobody wanted it!  */
    }
  }
  if(ListEmpty(&bnd->stripLst)) {
    bnd->strip = NULL;
    bnd->data  = NULL;
    if(bnd->final)
      disable_dst(flo,pet,bnd);
  }
  return(suspend != 0);
}                               /* end put_data */


/*------------------------------------------------------------------------
---------- free strip(s) from a receptor band or an emitter band ---------
------------------------------------------------------------------------*/
static void free_data(
     floDefPtr   flo,
     peTexPtr    pet,
     bandPtr     bnd)
{
  bandMsk  msk = 1<<bnd->band;
  
  /* free strips until we run out or reach the one we're working in
   */
  while(_release_ok(bnd)) {
    stripPtr strip;

    RemoveMember(strip, bnd->stripLst.flink);
    bnd->available -= strip->length - (bnd->minGlobal - strip->start);
    bnd->minGlobal  = strip->end + 1;
    if(bnd->isInput && bnd->receptor->forward & msk)
      forward_strip(flo,pet,strip);
    else
      free_strip(flo,strip);
  }
  /* a little bookkeeping to let the scheduler know where we're at
   */
  bnd->available -= bnd->current - bnd->minGlobal;
  bnd->minGlobal  = bnd->current;
  if(bnd->isInput) {
    CheckSrcReady(bnd,msk);
  }
  if(bnd->final && bnd->isInput && ListEmpty(&bnd->stripLst)) {
    bnd->receptor->active &= ~msk;
    bnd->receptor->attend &= ~msk;
  }
  if(!(bnd->data = _is_local(bnd) ? _line_ptr(bnd) : NULL))
    bnd->strip = NULL;
}                               /* end free_data */


/*------------------------------------------------------------------------
---- take list of strips passed in (most likely from a photomap), clone --
---- the headers, and hang them on the input receptor to make the data  --
---- accessable by standard macros					--
------------------------------------------------------------------------*/
static Bool import_strips(
     floDefPtr	 flo,
     peTexPtr    pet,
     bandPtr     bnd,
     stripLstPtr strips)
{
  stripPtr  strip = strips->flink, clone = NULL;
  receptorPtr rcp = pet->receptor; 
  CARD8 msk = 1<<bnd->band;
  
  for(strip = strips->flink; !ListEnd(strip,strips); strip = strip->flink) {
    
    if(!(clone = clone_strip(flo, strip)))
      AllocError(flo,pet->peDef, return(FALSE));
    
    clone->format = bnd->format;	  /* this had better be right! */
    
    bnd->available += clone->length;
    
    InsertMember(clone,bnd->stripLst.blink);
  }
  bnd->final     = clone->final;
  bnd->maxGlobal = clone->end + 1;
  
  if(!(rcp->admit &= ~msk))
    --pet->admissionCnt;
  
  return(TRUE);
}                               /* end import_strips */


/*------------------------------------------------------------------------
--------- Clone a strip and pass it on to the element's recipients -------
------------------------------------------------------------------------*/
static Bool pass_strip(
     floDefPtr	flo,
     peTexPtr   pet,
     bandPtr    bnd,
     stripPtr strip)
{
  stripPtr clone;

  if(!(pet->emitting & 1<<bnd->band))
    return(TRUE);			  /* output disabled */

  if(!(clone = clone_strip(flo, strip)))
    AllocError(flo,pet->peDef, return(FALSE));

  clone->format = bnd->format;		  /* this had better be right!	  */

  put_strip(flo,pet,clone);		  /* give to downstream receptors */
  if(!clone->flink)
    free_strip(flo,clone);		  /* nobody wanted it */

  if ((bnd->final = strip->final) != 0)
    disable_dst(flo,pet,bnd);

  return(TRUE);
}                               /* end pass_strip */


/*------------------------------------------------------------------------
------------ see if it's ok to over write the data in a src strip --------
------------------------------------------------------------------------*/
static Bool alter_src(
     floDefPtr flo,
     peTexPtr  pet,
     stripPtr  strip)
{
  stripPtr chk;
  
  if(!strip->data || strip->Xowner)
    return(FALSE);

  /* make sure there are no other users of this strip's data
   */
  for(chk = strip; chk->parent && chk->refCnt == 1; chk = chk->parent);
  return(chk->refCnt == 1);
}                               /* end alter_src */


/*------------------------------------------------------------------------
--------- pass all remaining input for this band straight through --------
------------------------------------------------------------------------*/
static void bypass_src(
     floDefPtr	flo,
     peTexPtr	pet,
     bandPtr   sbnd)
{
  stripPtr  strip;
  bandPtr    dbnd = &pet->emitter[sbnd->band];
  CARD8      *src;
  CARD8      *dst;

  if(sbnd->receptor->active & 1<<sbnd->band) {
    /*
     * if there's lingering data, see that it gets to its destination
     */
    for(src = (CARD8*)GetCurrentSrc(flo,pet,sbnd),
	dst = (CARD8*)GetCurrentDst(flo,pet,dbnd);
	src && dst;
	src = (CARD8*)GetNextSrc(flo,pet,sbnd,KEEP),
	dst = (CARD8*)GetNextDst(flo,pet,dbnd,!src)) {
      if(src != dst)
	memcpy((char*)dst, (char*)src, (int)dbnd->pitch);
    }
    /* if there's a partial strip still here, adjust its length
     */
    if(!ListEmpty(&dbnd->stripLst)) {
      strip = dbnd->stripLst.blink;
      if(strip->start < dbnd->current) {
	strip->end    = dbnd->current - 1;
	strip->length = dbnd->current - strip->start;
	put_data(flo,pet,dbnd);
      }
    }
    /* shut down the src band, or the dst band if we're all done
     */
    if(pet->emitting &= ~(1<<dbnd->band))
      disable_src(flo,pet,sbnd,FLUSH);
    else
      disable_dst(flo,pet,dbnd);
  }    
  /* if we're still accepting input, the remainder will bypass this element
   */
  sbnd->receptor->bypass |= 1<<sbnd->band;
}                               /* end bypass_src */


/*------------------------------------------------------------------------
---------- disable src band and discard any remaining input data ---------
------------------------------------------------------------------------*/
static void disable_src(
     floDefPtr	flo,
     peTexPtr	pet,
     bandPtr	bnd,
     Bool	purge)
{
  bandMsk msk = 1<<bnd->band;

  if(bnd->receptor->admit & msk && pet->peDef->flags.putData)
    --flo->floTex->imports;		/* one less import client band    */

  if(bnd->receptor->admit && !(bnd->receptor->admit &= ~msk))
    --pet->admissionCnt;		/* one less receptor needing data */

  bnd->final = TRUE;

  if(purge)
    FreeData(flo,pet,bnd,bnd->maxGlobal);
}                               /* end disable_src */


/*------------------------------------------------------------------------
--- disable dst band -- also disables all src's if no dst bands remain ---
------------------------------------------------------------------------*/
static void disable_dst(
     floDefPtr	flo,
     peTexPtr	pet,
     bandPtr	dbnd)
{
  peDefPtr    ped = pet->peDef;
  receptorPtr rcp, rend = &pet->receptor[ped->inCnt];
  bandMsk mask;
  bandPtr sbnd;

  /* if this is the last emitter band to turn off and this isn't an import 
   * client element, we'll step thru all the receptor bands and kill them too
   */
  if(!(pet->emitting &= ~(1<<dbnd->band)) && !ped->flags.putData)
    for(rcp = pet->receptor; rcp < rend; ++rcp)
      for(mask = 1, sbnd = rcp->band; rcp->active; mask <<= 1, ++sbnd)
	if(rcp->active & mask)
	  disable_src(flo,pet,sbnd,TRUE);
  if(ped->flags.getData) {
    ped->outFlo.active &= ~(1<<dbnd->band);
    if(!(ped->outFlo.ready & 1<<dbnd->band))
      flo->floTex->exports--;
  }
}                               /* end disable_dst */
/*------------------------------------------------------------------------
- Get permission for an emitter to write into an existing receptor strip -
------------------------------------------------------------------------*/
static stripPtr	alter_data(
     floDefPtr flo,
     peTexPtr  pet,
     bandPtr   db)
{
  bandPtr  sb = db->inPlace;
  stripPtr chk, strip = sb->strip ? sb->strip : sb->stripLst.flink;
  
  /* search through the source data for the corresponding line number
   */
  while(!ListEnd(strip,&sb->stripLst))
    if(db->current > strip->end)
      strip = strip->flink;
    else if(db->current < strip->start)
      strip = strip->blink;
    else if(!strip->data || strip->Xowner)
      break;
    else {
      /* make sure there are no other users of this strip's data
       */
      for(chk = strip; chk->parent && chk->refCnt == 1; chk = chk->parent);
      if(chk->refCnt > 1)
	break;
      return(clone_strip(flo,strip));    /* return a clone of the src strip */
    }
  return(NULL);
}                               /* end alter_data */


/*------------------------------------------------------------------------
---------- enter with a strip containing at least one byte of data  ------
----------    {if a new strip must be created (to hold at least     ------
----------     contig bytes), available data will be copied to it}  ------
---------- return with a strip containing "contig" contiguous bytes ------
------------------------------------------------------------------------*/
static stripPtr contig_data(
     floDefPtr	flo,
     peTexPtr	pet,
     bandPtr	bnd,
     stripPtr   i_strip,
     CARD32     contig)
{
  stripPtr o_strip, n_strip;
  CARD32   limit, start, skip, avail = i_strip->end - bnd->current + 1;
  
  if(contig <= i_strip->bufSiz - i_strip->length + avail)
    o_strip = i_strip;
  else {
    /* i_strip too small, make a new one and copy available data into it
     */
    if(!(o_strip = make_strip(flo, bnd->format, bnd->current, avail,
			      contig + Align(contig, flo->floTex->stripSize),
			      TRUE)))
      AllocError(flo,pet->peDef, return(NULL));
    InsertMember(o_strip,i_strip);
    memcpy((char*)o_strip->data,
	   (char*)&i_strip->data[bnd->current-i_strip->start], (int)avail);
    if(i_strip->length -= avail)
       i_strip->end    -= avail;
    else {
      RemoveMember(n_strip,i_strip);
      o_strip->final = n_strip->final;
      free_strip(flo, n_strip);
    }
  }
  /* determine how far we can extend our o_strip
   */
  if(bnd->current + contig <= bnd->maxGlobal)
    limit = bnd->current + contig;		/* limit to data needed     */
  else
    limit = bnd->maxGlobal;			/* limit to data available  */

  /* if there are more strips beyond "o_strip", transfer the needed amount of
   * data into our mega-strip (free any strips that we completely consume)
   */
  for(start = o_strip->end+1; start < limit; start += avail) {
    n_strip = o_strip->flink;
    skip    = start - n_strip->start;
    avail   = min(n_strip->length - skip, limit - start);
    memcpy((char*)&o_strip->data[o_strip->length],
	   (char*)&n_strip->data[skip], (int)avail);
    o_strip->end    += avail;
    o_strip->length += avail;
    if(avail+skip == n_strip->length) {
      RemoveMember(n_strip, n_strip);
      o_strip->final = n_strip->final;
      free_strip(flo, n_strip);
    }
  }
 if(!bnd->isInput) {
    limit = bnd->current + contig;
    bnd->available += limit - bnd->maxGlobal;
    bnd->maxGlobal  = limit;
    o_strip->end    = limit - 1;
    o_strip->length = limit - o_strip->start;
  }
  return(o_strip);
}                               /* end contig_data */


/*------------------------------------------------------------------------
---------------------------- Make a new strip ----------------------------
------------------------------------------------------------------------*/
static stripPtr	make_strip(
     floDefPtr flo,
     formatPtr fmt,
     CARD32    start,
     CARD32    units,
     CARD32    bytes,
     Bool      allocData)
{
  stripPtr    strip;
  stripLstPtr cache = &flo->floTex->stripHead;
  Bool     cachable = allocData && bytes == flo->floTex->stripSize;

  if(!ListEmpty(cache) && (cachable || !cache->blink->data))
    RemoveMember(strip, cachable ? cache->flink : cache->blink);
  else if ((strip = (stripPtr) XieMalloc(sizeof(stripRec))) != 0) {
      strip->data  = NULL;
      ++STRIPS; /*DEBUG*/
  }
  if(strip) {
    strip->flink   = NULL;
    strip->parent  = NULL;
    strip->format  = fmt;
    strip->refCnt  = 1;
    strip->Xowner  = !allocData;
    strip->canonic = IsCanonic(fmt->class);
    strip->final   = FALSE;
    strip->cache   = cachable;
    strip->start   = start;
    strip->end     = start + units - 1;
    strip->length  = units;
    strip->bitOff  = 0;
    strip->bufSiz  = bytes;

    if(allocData && bytes && !strip->data)
      if ((strip->data = (CARD8 *) XieCalloc(bytes)) != 0) /* calloc to hush purify */
	BYTES += bytes; /*DEBUG*/
      else
	strip  = free_strip(NULL,strip);
  }
  return(strip);
}                               /* end make_strip */


/*------------------------------------------------------------------------
---- Clone a new modifiable strip wrapper for existing read-only data ----
------------------------------------------------------------------------*/
static stripPtr clone_strip(floDefPtr flo, stripPtr in_strip)
{
  stripLstPtr  cache = &flo->floTex->stripHead;
  stripPtr out_strip;

  if(ListEmpty(cache) || cache->blink->data) {
    out_strip = (stripPtr) XieMalloc(sizeof(stripRec));
    ++STRIPS; /*DEBUG*/
  } else {
    RemoveMember(out_strip, cache->blink);
  }
  if(out_strip) {
    *out_strip         = *in_strip;
     out_strip->flink  =  NULL;
     out_strip->parent =  in_strip;
     out_strip->refCnt =  1;
    ++in_strip->refCnt;
  }
  return(out_strip);
}                               /* end clone_strip */


/*------------------------------------------------------------------------
---------------- Put strip on each receptor fed by an element ------------
------------------------------------------------------------------------*/
static bandMsk put_strip(floDefPtr flo, peTexPtr pet, stripPtr strip)
{
  peTexPtr    dst;
  inFloPtr    inf;
  bandPtr     bnd;
  stripPtr  clone;
  receptorPtr rcp;
  CARD8      band = strip->format->band;
  bandMsk  repmsk, mask = 1<<band, suspend = 0;

  if(pet->peDef->flags.export) {
    pet->outFlo->ready |= mask;
    /*
     * give to DIXIE via our outFlo; if we're not first in line, make a clone
     */
    if(!(clone = strip->flink ? clone_strip(flo,strip) : strip))
      AllocError(flo,pet->peDef, return(suspend));
    InsertMember(clone,pet->outFlo->output[band].blink);
    ++flo->floTex->putCnt;
    return(suspend);
  }
  /* hang this strip on the receptor of each of our interested recipients
   */
  for(inf = pet->outFlo->outChain; inf; inf = inf->outChain) {
    if(inf->format[band].class != strip->format->class)
      continue;
    dst =  inf->ownDef->peTex;
    rcp = &dst->receptor[inf->index];
    bnd = &rcp->band[band];
      
    if(rcp->bypass & mask) {
      suspend |= put_strip(flo,dst,strip);	/* just passin' through */
      continue;
    } else if(!(rcp->admit & mask)) {
      continue;					/* data not wanted here */
    }
    if(strip->final && !(rcp->admit &= ~mask))
      --dst->admissionCnt;			/* all bands complete   */
    /*
     * give the data to the intended receptor band
     */
    for(repmsk = mask; repmsk <= rcp->active; ++bnd, repmsk <<= 1) {
      if(rcp->active & repmsk) {
	bnd->final      = strip->final;
	bnd->maxGlobal  = strip->end + 1;
	bnd->available += strip->length;
	Schedule(flo,dst,rcp,bnd,repmsk);	/* schedule if runnable */
	suspend |= dst->scheduled;
	/*
	 * first recipient gets the original, all others get clones
	 */
	if(!(clone = strip->flink ? clone_strip(flo,strip) : strip))
	  AllocError(flo,dst->peDef, break);
	InsertMember(clone,bnd->stripLst.blink);
	++flo->floTex->putCnt;
      }
      /* see if we should replicate the data through the other bands
       */
      if(!rcp->band[0].replicate) break;
    }
  }
  return(suspend);
}                               /* end put_strip */


/*------------------------------------------------------------------------
------------ forward input data downstream (re-buffer if Xowner)  --------
------------ primarily used to pass compressed data to a photomap --------
------------------------------------------------------------------------*/
static void forward_strip(floDefPtr flo, peTexPtr pet, stripPtr fwd)
{
  if(!pet->peDef->flags.putData) {
    /* non-client data -- just pass it along
     */
    fwd->flink = NULL;
    put_strip(flo,pet,fwd);		  /* give to downstream receptors */
    if(!fwd->flink)
      free_strip(flo,fwd);		  /* hmm, nobody wanted it	  */
  } else {
    /* since this is client data we must copy it before passing it along
     */
    stripLstPtr lst = &pet->outFlo->output[fwd->format->band];
    stripPtr    tmp =  ListEmpty(lst) ? NULL : lst->flink;
    int      maxlen =  flo->floTex->stripSize, datlen;
    int	    overlap =  tmp ? tmp->end - fwd->start + 1 : 0;
    int       start =  fwd->start  + overlap;
    int        size =  fwd->length - overlap;
    CARD8     *data = &fwd->data[overlap];
    Bool      final = FALSE;

    while(!final) {
      if(ListEmpty(lst))
	if ((tmp = make_strip(flo, fwd->format, start, 0, maxlen, TRUE)) != 0) {
	  InsertMember(tmp,lst->flink);
	} else {
	  free_strip(flo,fwd);
	  AllocError(flo,pet->peDef, return);
	}
      if(size) {
	datlen = min(size, tmp->bufSiz - tmp->length);
	memcpy((char*)&tmp->data[tmp->length], (char*)data, (int)datlen);
	tmp->length += datlen;
	tmp->end    += datlen;
	data        += datlen;
	size        -= datlen;
      }
      if((!size && fwd->final) || tmp->length == tmp->bufSiz) {
	RemoveMember(tmp,lst->flink);
	start        = tmp->start + tmp->length;
	tmp->final   = final = fwd->final && !size;
	tmp->canonic = fwd->canonic;
	tmp->flink   = NULL;
	put_strip(flo,pet,tmp);		  /* give to downstream receptors */
	if(!tmp->flink)
	  free_strip(flo,tmp);
      }
      if(!size)   break;
    }
    free_strip(flo,fwd);
  }
}                               /* end forward_strip */


/*------------------------------------------------------------------------
----------------------------- Get rid of a strip -------------------------
------------------------------------------------------------------------*/
static stripPtr free_strip(floDefPtr flo, stripPtr strip)
{
  lstPtr cache;

  if(strip && !--strip->refCnt) {
    /* since this was the last reference we're going to nuke this strip,
     * if this was a clone, free the parent and forget where the data was
     */
    if(strip->parent) {
      free_strip(flo, strip->parent);
      strip->data = NULL;
    } else if(strip->data) {
      /* if the data buffer belongs to coreX or is uncachable, nuke it */
      if(strip->Xowner)
	strip->data = NULL;
      else if(!flo || !strip->cache) {
	strip->data = (CARD8 *) XieFree(strip->data);
	BYTES -= strip->bufSiz; /*DEBUG*/
      }
    }
    if(flo) {
      /* strips with standard data buffers are cached at the front
       * strips without data buffers go at the back
       */
      strip->refCnt = 1;
      strip->parent = NULL;
      cache = (strip->data
	       ? (lstPtr)&flo->floTex->stripHead
	       : (lstPtr) flo->floTex->stripHead.blink);
      InsertMember(strip,cache);
    } else {
      XieFree(strip);
      --STRIPS; /*DEBUG*/
    }
  }
  return(NULL);
}                               /* end free_strip */

/* end module strip.c */
