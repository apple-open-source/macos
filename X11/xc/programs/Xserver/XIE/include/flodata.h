/* $Xorg: flodata.h,v 1.4 2001/02/09 02:04:23 xorgcvs Exp $ */
/**** module flodata.h ****/
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
*******************************************************************************

	flodata.h - data formats 

	Dean Verheiden, Robert NC Shelley -- AGE Logic, Inc. April 1993

******************************************************************************/

#ifndef _XIE_FLODATA
#define _XIE_FLODATA

#include <flo.h>

/*
 * Supported data format classes
 */
#define	BIT_PIXEL	  1	/* bitonal data				*/
#define BYTE_PIXEL	  2	/* up to 2^8  levels			*/
#define PAIR_PIXEL	  3	/* up to 2^16 levels			*/
#define	QUAD_PIXEL	  4	/* up to 2^32 levels			*/
#define UNCONSTRAINED	 16	/* levels are undefined 		*/
#define LUT_ARRAY	 32	/* non-canonic lut array		*/
#define RUN_LENGTH	 64	/* non-canonic run_length		*/
#define STREAM		128	/* non-canonic generic stream		*/

#define IsntLut(dfc)         (dfc !=  LUT_ARRAY)
#define IsntDomain(dfc)      (dfc & ~(RUN_LENGTH | BIT_PIXEL))
#define IsntCanonic(dfc)     (dfc &  (RUN_LENGTH | STREAM | LUT_ARRAY))
#define IsntConstrained(dfc) \
	      (dfc & (UNCONSTRAINED | RUN_LENGTH | STREAM | LUT_ARRAY))

#define IsLut(dfc)         (dfc == LUT_ARRAY)
#define IsDomain(dfc)      (!IsntDomain(dfc))
#define IsCanonic(dfc)     (!IsntCanonic(dfc))
#define IsConstrained(dfc) (!IsntConstrained(dfc))

#define IndexClass(dfc)    (dfc == UNCONSTRAINED ? 0 : dfc )

#define ConstrainConst(fconst,levels) \
	( (fconst <= 0.)	? (CARD32) 0 : \
	  (fconst >= levels)	? (CARD32) (levels - 1) : \
				  (CARD32) (fconst + 0.5))

/*
 * Data types and sizes for supported format classes
 */
typedef	CARD8	BitPixel;
typedef	CARD8	BytePixel;
typedef	CARD16	PairPixel;
typedef	CARD32	QuadPixel;
typedef	float	RealPixel;		/* type of Unconstrained data	*/

#define sz_BitPixel		 1	
#define sz_BytePixel		 8	
#define sz_PairPixel		16	
#define sz_QuadPixel		32
#define sz_RealPixel		32	/* size of Unconstrained data	*/

typedef struct _format {
  CARD8   class;	/* format class {e.g. BIT_PIXEL, STREAM, ...}	*/
  CARD8	  band;		/* band number {0,1,2}				*/
  BOOL    interleaved;	/* true if pixels contain multiple bands	*/
  CARD8   depth;	/* minimum bits needed to contain levels	*/
  CARD32  width;	/* width in pixels				*/
  CARD32  height;	/* height in pixels				*/
  CARD32  levels;	/* quantization levels				*/
  CARD32  stride;	/* distance between adjacent pixels in bits	*/
  CARD32  pitch;	/* distance between adjacent scanlines in bits	*/
} formatRec, *formatPtr;


typedef struct _strip {
  struct _strip *flink;	/* link to next strip				  */
  struct _strip *blink;	/* link to previous strip			  */
  struct _strip *parent;/* link to strip from which this one was cloned   */
  formatRec  *format;	/* pointer to format record that describes data	  */
  CARD32      refCnt;	/* reference count				  */
  BOOL	      Xowner;	/* if true, core X "owns" the data buffer	  */
  BOOL        canonic;	/* if true, units are scanlines, otherwise bytes  */
  BOOL        final;	/* if true, this is the last strip for this band  */
  BOOL        cache;	/* if true, buffer can be cached (standard size)  */
  CARD32      start;	/* first line/byte of overall data in this strip  */
  CARD32      end;	/* last  line/byte of overall data in this strip  */
  CARD32      length;	/* lines/bytes of useable data in buffer	  */
  CARD32      bitOff;	/* bit offset to first data (usually zero)	  */
  CARD32      bufSiz;	/* size of the data buffer in bytes		  */
  CARD8      *data;	/* pointer to the data buffer			  */
} stripRec, *stripPtr;

/* generic header for managing circular doubly linked lists
 */
typedef struct _lst {
    struct _lst *flink;
    struct _lst *blink; 
} lstRec, *lstPtr;

/* link-pair for managing a circular doubly linked list of strips
 */
typedef struct _striplst {
  stripRec  *flink;
  stripRec  *blink; 
} stripLstRec, *stripLstPtr;

#endif /* end _XIE_FLODATA */
