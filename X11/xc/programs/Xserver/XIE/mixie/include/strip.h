/* $Xorg: strip.h,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/**** module strip.h ****/
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
******************************************************************************

	strip.h -- DDXIE machine independent data manager definitions

	Robert NC Shelley -- AGE Logic, Inc.  May 1993

******************************************************************************/

#ifndef _XIEH_STRIP
#define _XIEH_STRIP

#define STANDARD_STRIP_SIZE 8192	/* temporary hack */

/* data management interface */
typedef struct _stripvec {
  xieDataProc	make_bytes;
  xieDataProc	make_lines;
  xieBoolProc	map_data;
  xieDataProc	get_data;
  xieBoolProc	put_data;
  xieVoidProc	free_data;
  xieBoolProc	pass_strip;
  xieBoolProc	import_strips;
  xieBoolProc	alter_src;
  xieVoidProc	bypass_src;
  xieVoidProc	disable_src;
  xieVoidProc	disable_dst;
} stripVecRec;

/* per-band scanline/byte data management info
 */
typedef struct _band {
  stripLstRec       stripLst;	/* strips available for processing          */
  stripPtr	    strip;	/* strip of most recent access              */
  CARD8	           *data;	/* pointer to most recent line/byte access  */
  CARD32	    minGlobal;	/* lowest available line/byte in stripLst   */
  CARD32	    minLocal;	/* lowest available line/byte in strip      */
  CARD32	    current;	/* line/byte of most recent access          */
  CARD32	    maxLocal;	/* one beyond highest line/byte in strip    */
  CARD32	    maxGlobal;	/* one beyond highest line/byte in stripLst */
  CARD32	    pitch;	/* number of bytes to start of next line    */
  CARD32	    mapSize;	/* size of dataMap                          */
  CARD8	          **dataMap;	/* pointers for multi-line random access    */
  CARD32	    threshold;	/* line/bytes needed for execution     	    */
  CARD32	    available;	/* line/bytes available in stripLst	    */
  bandMsk	    replicate;	/* replicate data from band 0 to bands 1 & 2*/
  CARD8		    band;	/* band number				    */
  BOOL		    final;	/* true when last line/byte is in stripLst  */
  BOOL		    isInput;	/* band type: true = input, false = output  */
  struct _receptor *receptor;	/* receptor if isInput, otherwise NULL      */
  formatPtr	    format;	/* common format info for all strips	    */
  struct _band     *inPlace;	/* what to clone to make in-place strips    */
  pointer	    pcroi;	/* pointer to ROI entry for current line    */ 
  CARD32	    xindex;	/* index into pcroi ROI for current x pos   */
  INT32	    	    xcount;	/* current x location in output band	    */
  BOOL		    ypass;	/* True = no processing domain this line    */
  BOOL		    inside;	/* indicates if inside or outside domain    */
  BOOL		    allpass;	/* indicates no lines fall in ROI	    */ 
  CARD8		    pad;
} bandRec, *bandPtr;

/**************************************************************************
 * Photoflo data manager convenience macros
 *
 * pointer GetCurrentSrc(flo,pet,bnd)			single Src line/byte
 * pointer GetCurrentDst(flo,pet,bnd)			single Dst line/byte
 * pointer GetNextSrc(flo,pet,bnd,purge)		single Src line/byte
 * pointer GetNextDst(flo,pet,bnd,purge)		single Dst line/byte
 * pointer GetSrc(flo,pet,bnd,unit,purge)		single Src line/byte
 * pointer GetDst(flo,pet,bnd,unit,purge)		single Dst line/byte
 * pointer GetSrcBytes(flo,pet,bnd,unit,len,purge)	contiguous Src bytes
 * pointer GetDstBytes(flo,pet,bnd,unit,len,purge)	contiguous Dst bytes
 * Bool    MapData(flo,pet,bnd,map,unit,len,purge)	map  multi-lines/bytes
 * Bool    PutData(flo,pet,bnd,unit)			put  multi-lines/bytes
 * void    FreeData(flo,pet,bnd,unit)			free multi-lines/bytes
 * Bool    PassStrip(flo,pet,bnd,strip)			forward clone of strip
 * Bool    ImportStrips(flo,pet,bnd,strips)		import Photomap strips
 * Bool    AlterSrc(flo,pet,strip)			see if Src is mutable
 * void    BypassSrc(flo,pet,bnd)			bypass element for bnd
 * void    DisableSrc(flo,pet,bnd,purge)		disable input for bnd
 * void    DisableDst(flo,pet,bnd)			signal no bnd output
 *
 * key to common arguments:
 *	flo	floDefPtr
 *	pet	peTexPtr
 *	bnd	bandPtr
 *	unit	line number within canonic image, or
 *		byte number within non-canonic data stream
 *	len	number of units (lines or bytes)
 *	purge	if the requested data is not in the current strip and purge is
 *		true, all data preceding the first line or byte in the current
 *		request is freed (receptor band) or forwarded (emitter band).
 *	  NOTE: purge does not update scheduler info, so FreeData should be
 *		called for receptor bands prior to returning from the element.
 *
 * Additional macros:
 *	AttendBand(bnd) ------- scheduler will consider this band (default)
 *	IgnoreBand(bnd) ------- scheduler will ignore this band
 *	AttendReceptor(rcp) --- scheduler will consider this receptor (default)
 *	IgnoreReceptor(rcp) --- scheduler will ignore this receptor
 *      SetBandFinal(bnd) ----- set final "true" for band and current strip
 *      SetBandThreshold(bnd,value) -- set new threshold value
 *      TruncateStrip(bnd,value) ----- adjust end of strip to value
 **************************************************************************/

/* utility macros for strip manager internal use only
 */
#define _is_local(bnd)		   ((bnd)->minLocal    <= (bnd)->current && \
				    (bnd)->current     <  (bnd)->maxLocal)
#define _is_global(bnd)		   ((bnd)->minGlobal   <= (bnd)->current && \
				    (bnd)->current     <  (bnd)->maxGlobal)
#define _is_local_contig(bnd,len)  ((bnd)->minLocal    <= (bnd)->current && \
				    (bnd)->current+len <= (bnd)->maxLocal)
#define _is_global_contig(bnd,len) ((bnd)->minGlobal   <= (bnd)->current && \
				    (bnd)->current+len <= (bnd)->maxGlobal)
#define _byte_ptr(bnd) \
	 (&(bnd)->strip->data[(bnd)->current - (bnd)->strip->start])
#define _line_ptr(bnd) \
	 (&(bnd)->strip->data \
	  [(bnd)->pitch * ((bnd)->current - (bnd)->strip->start)])
#define _release_ok(bnd) \
	 (!ListEmpty(&(bnd)->stripLst) && \
	  ((bnd)->current > (bnd)->stripLst.flink->end || !(bnd)->maxGlobal))

/**************************************************************************
 * public data management macros for handling strip/scanline/other data
 */
#define AttendBand(bnd) ((bnd)->receptor->attend |= \
			 (bnd)->receptor->active & 1<<(bnd)->band)
#define AttendReceptor(rcp) (rcp->attend = rcp->active)
#define IgnoreBand(bnd) ((bnd)->receptor->attend &= ~(1<<(bnd)->band))
#define IgnoreReceptor(rcp) (rcp->attend = NO_BANDS)
#define SetBandFinal(bnd) ((bnd)->strip \
			   ? ((bnd)->final = (bnd)->strip->final = TRUE) \
			   : ((bnd)->final = TRUE))
#define SetBandThreshold(bnd,value) \
		if( ((bnd)->threshold = value) > (bnd)->available) \
		     (bnd)->receptor->ready &= ~(1<<(bnd)->band); \
		else (bnd)->receptor->ready |=   1<<(bnd)->band
#define TruncateStrip(bnd,value) \
		{(bnd)->current = (value); \
		 if((bnd)->strip && _is_local(bnd)) { \
		   int q = (bnd)->maxLocal - (value); \
		   (bnd)->strip->length -= q; (bnd)->strip->end -= q; \
		   (bnd)->available -= q + (bnd)->maxGlobal - (bnd)->maxLocal;\
		   (bnd)->maxGlobal  = (bnd)->maxLocal = value; }}

/* return the current line/byte pointer (NULL if not currently available)
 *	 floDefPtr flo; peTexPtr pet; bandPtr bnd;
 */
#define GetCurrentSrc(flo,pet,bnd) \
	(pointer)((bnd)->data ? (bnd)->data \
		: _is_global(bnd) \
		? (*flo->stripVec->get_data)(flo,pet,bnd,1,FALSE) \
		: ((bnd)->data = NULL))
#define GetCurrentDst(flo,pet,bnd) \
	(pointer)((bnd)->data ? (bnd)->data \
		: (*flo->stripVec->make_lines)(flo,pet,bnd,FALSE))

/* return the next sequential line/byte pointer (NULL if not available)
 *	 floDefPtr flo; peTexPtr pet; bandPtr bnd; Bool purge;
 */
#define GetNextSrc(flo,pet,bnd,purge) \
	(pointer)(++(bnd)->current < (bnd)->maxLocal \
		? ((bnd)->data += (bnd)->pitch) \
		: _is_global(bnd) \
		? (*flo->stripVec->get_data)(flo,pet,bnd,1,purge) \
		: ((bnd)->data = NULL))
#define GetNextDst(flo,pet,bnd,purge) \
	(pointer)(++(bnd)->current < (bnd)->maxLocal \
		? ((bnd)->data += (bnd)->pitch) \
		: (*flo->stripVec->make_lines)(flo,pet,bnd,purge))

/* return the specified line/byte pointer (random access)
 * (returns NULL if not available)
 *	floDefPtr flo; peTexPtr pet; bandPtr bnd; CARD32 unit; Bool purge;
 */
#define GetSrc(flo,pet,bnd,unit,purge) \
	(pointer)((bnd)->current = unit, _is_local(bnd) \
		? ((bnd)->data = _line_ptr(bnd)) \
		: _is_global(bnd) \
		? (*flo->stripVec->get_data)(flo,pet,bnd,1,purge) \
		: ((bnd)->data = NULL))
#define GetDst(flo,pet,bnd,unit,purge) \
	(pointer)((bnd)->current = unit, _is_local(bnd) \
		? ((bnd)->data = _line_ptr(bnd)) \
		: (*flo->stripVec->make_lines)(flo,pet,bnd,purge))

/* return the specified byte pointer with len contiguous bytes available
 * (returns NULL if not available)
 *	floDefPtr flo; peTexPtr pet; bandPtr bnd; CARD32 unit, len; Bool purge;
 *	len -- number of contiguous bytes required
 */
#define GetSrcBytes(flo,pet,bnd,unit,len,purge) \
	(pointer)((bnd)->current = unit, _is_local_contig(bnd,len) \
		? ((bnd)->data = _byte_ptr(bnd)) : _is_global(bnd) \
		? (*flo->stripVec->get_data)(flo,pet,bnd,len,purge) \
		: ((bnd)->data = NULL))
#define GetDstBytes(flo,pet,bnd,unit,len,purge) \
	(pointer)((bnd)->current = unit, _is_local_contig(bnd,len) \
		? ((bnd)->data = _byte_ptr(bnd)) \
		: (*flo->stripVec->make_bytes)(flo,pet,bnd,len,purge))

/* load the data map with pointers to a range of lines
 * upon return, bnd info points to unit (the first line mapped)
 * returns TRUE if all requested lines are available, otherwise FALSE
 *	floDefPtr flo; peTexPtr pet; bandPtr bnd;
 *	CARD32 map, unit, len; Bool purge;
 *	map --- first dataMap index to use (normally 0),
 *	unit -- first line/byte number to map,
 *	len --- number of lines/bytes to map;
 */
#define MapData(flo,pet,bnd,map,unit,len,purge) \
	(*flo->stripVec->map_data)(flo,pet,bnd,map,unit,len,purge)

/* release lines/bytes to downstream recipients (up to but not including unit),
 * bnd->data is updated if unit is in current strip (else it is NULLed)
 * returns TRUE if the element should suspend itself
 *	floDefPtr flo; peTexPtr pet; bandPtr bnd; CARD32 unit;
 */
#define PutData(flo,pet,bnd,unit) \
	((bnd)->data = (bnd)->current == unit ? (bnd)->data \
	 : ((bnd)->current = unit, _is_local(bnd) ? _line_ptr(bnd) : NULL), \
	 _release_ok(bnd) ? (*flo->stripVec->put_data)(flo,pet,bnd) : FALSE)

/* free lines/bytes (up to but not including unit)
 * upon return, bnd info points to unit, if previously available
 *	floDefPtr flo; peTexPtr pet; bandPtr bnd; CARD32 unit;
 */
#define	FreeData(flo,pet,bnd,unit) \
	((bnd)->current = unit, (*flo->stripVec->free_data)(flo,pet,bnd))

/* import a list of strips from a Photomap
 *	floDefPtr flo; peTexPtr pet; bandPtr bnd; stripLstPtr strips;
 */
#define ImportStrips(flo,pet,bnd,strips) \
	(*flo->stripVec->import_strips)(flo,pet,bnd,strips)

/* clone a strip and pass it on to downstream elements
 *	floDefPtr flo; peTexPtr pet; bandPtr bnd; stripPtr strip;
 */
#define PassStrip(flo,pet,bnd,strip) \
	(*flo->stripVec->pass_strip)(flo,pet,bnd,strip)

/* check Src strip to see if its data can be over written
 *	floDefPtr flo; peTexPtr pet; bandPtr bnd; stripPtr strip;
 */
#define AlterSrc(flo,pet,strip) \
	(*flo->stripVec->alter_src)(flo,pet,strip)

/* flush data from bnd to downstream elements, then set bnd in bypass mode
 *	floDefPtr flo; peTexPtr pet; bandPtr bnd;
 */
#define BypassSrc(flo,pet,bnd) \
	(*flo->stripVec->bypass_src)(flo,pet,bnd)

/* disable any further input from bnd, and discard data if purge is true
 *	floDefPtr flo; peTexPtr pet; bandPtr bnd; Bool purge;
 */
#define DisableSrc(flo,pet,bnd,purge) \
	(*flo->stripVec->disable_src)(flo,pet,bnd,purge)

/* disable any further output from bnd (i.e. send final, but no data)
 *	floDefPtr flo; peTexPtr pet; bandPtr bnd;
 */
#define DisableDst(flo,pet,bnd) \
	(*flo->stripVec->disable_dst)(flo,pet,bnd)

#endif /* end _XIEH_STRIP */
