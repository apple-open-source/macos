/* $Xorg: tex.h,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/**** module tex.h ****/
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
******************************************************************************
  
	tex.h -- generic device dependent photoflo definitions
  
	Dean Verheiden -- AGE Logic, Inc. April 1993
  
*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/include/tex.h,v 1.5 2001/12/14 19:58:34 dawes Exp $ */

#ifndef _XIEH_TEX
#define _XIEH_TEX

/* symbolic constants -- for flo manager initialization routines
 */
	/* Band mask constants for specifying all posible bands or no bands.
	 */
#define ALL_BANDS  ((bandMsk)~0)
#define NO_BANDS   ((bandMsk) 0)

	/* Constant for specifying that in-place operations are not wanted
	 * (i.e. use new Dst buffers).  The alternative is to specify a Src
	 * (e.g. SRCtag or SRCt1) whose buffer contents can be replaced with
	 * the result.  When requesting a Dst line, the data manager will map
	 * the requested Dst line to the corresponding Src line (the Src and
	 * Dst canonic data types must match for this to work).
	 */
#define NO_INPLACE ((INT32)-1)

	/* Constant for specifying that no space needs to be allotted for a
	 * data manager DataMap (i.e. data will be accessed on a line by line
	 * basis).  The alternative is to specify the maximum number of lines
	 * that will be needed in the DataMap (anywhere from a few lines to
	 * the full image).
	 */
#define NO_DATAMAP ((CARD32) 0)

	/* Constant for specifying that the element does not require any
	 * private parameter space.  The alternative is to specify the number
	 * of bytes of private storage that will be needed for parameters etc.
	 * The private area is allocated as contiguous bytes beyond the peTex
	 * structure, and therefore should not be freed explicitly.
	 */
#define NO_PRIVATE ((CARD32) 0)

	/* Constants for specifying whether or not the data manager and
	 * scheduler should cooperate to keep inputs and/or bands in sync.
	 */
#define SYNC	   ((Bool) 1)
#define NO_SYNC	   ((Bool) 0)


/* symbolic constants -- for data manager macros
 */
	/* The following pair of constants can be supplied for the "purge"
	 * argument in many of the data manager's strip access macros.
	 *
	 * KEEP specifies that all data currently owned by the Src or Dst
	 * should be retained (either the data will be needed again or there
	 * is no advantage in releasing the data as it is consumed).
	 *
	 * FLUSH specifies that all data that precedes the current unit (line
	 * or byte) can be dispensed with.  Src data may be freed, whereas Dst
	 * data may be forwarded to downstream elements.  FLUSH is a suggestion
	 * rather than a command.  Whether or not the data is actually flushed
	 * depends on the crossing of strip boundaries.  If one or more strips
	 * precede the strip containing the current unit, they will be flushed.
	 * If a downstream element becomes runnable as a result of forwarding
	 * a Dst strip, the data manager will signal to the calling element
	 * that it should defer to the downstream element(s).
	 */
#define KEEP	((Bool) 0)
#define FLUSH	((Bool) 1)


#ifndef _XIEC_FLOMAN
struct _band;
struct _receptor;

extern	int	InitFloManager(floDefPtr flo);
extern	int	MakePETex(
			floDefPtr flo,
			peDefPtr ped,
			CARD32 extend,
			Bool inSync,
			Bool bandSync);
extern	Bool	InitReceptors(
			floDefPtr flo,
			peDefPtr ped,
			CARD32 mapSize,
			CARD32 threshold);
extern	Bool	InitReceptor(
			floDefPtr flo,
			peDefPtr ped,
			struct _receptor * rcp,
			CARD32 mapSize,
			CARD32 threshold,
			unsigned process,
			unsigned bypass);
extern	Bool	InitEmitter(
			floDefPtr flo,
			peDefPtr ped,
			CARD32 mapSize,
			INT32 inPlace);
extern	Bool	InitBand(
			floDefPtr flo,
			peDefPtr ped,
			struct _band * bnd,
			CARD32 mapSize,
			CARD32 threshold,
			INT32 inPlace);
extern  void    ResetBand(struct _band * bnd);
extern	void	ResetReceptors(peDefPtr ped);
extern	void	ResetEmitter(peDefPtr ped);
#endif

#ifndef _XIEC_SCHED
extern	int	InitScheduler(floDefPtr flo);
#endif

#ifndef _XIEC_STRIP
extern	int	InitStripManager(floDefPtr flo);
extern  int	DebriefStrips(stripLstPtr i_head, stripLstPtr o_head);
extern  void	FreeStrips(stripLstPtr head);
#endif

#endif /* end _XIEH_TEX */
