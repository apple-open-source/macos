/* $Xorg: flostr.h,v 1.4 2001/02/09 02:04:23 xorgcvs Exp $ */
/**** module flostr.h ****/
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
  
	flostr.h -- DIXIE photoflo and element structure definitions
  
	Robert NC Shelley -- AGE Logic, Inc. April 1993
  
*****************************************************************************/

#ifndef _XIEH_FLOSTR
#define _XIEH_FLOSTR

#include <flodata.h>

typedef CARD8 bandMsk;		/* per-band bit mask (bit0 == band0,...)*/

/* dixie photo element entry points
 */
typedef struct _dielemvec {
  xieBoolProc	prep;		/* prepare for analysis and execution	*/
  xieBoolProc   debrief;	/* post execution clean-up		*/
} diElemVecRec, *diElemVecPtr;

/* ddxie photo-element interface
 */
typedef struct _ddelemvec {
  xieIntProc	create;
  xieIntProc	initialize;
  xieIntProc	activate;
  xieIntProc	flush;
  xieIntProc	reset;
  xieIntProc	destroy;
} ddElemVecRec, *ddElemVecPtr;

/* ddxie photoflo manager entry points
 */
typedef struct _flovec {
  xieIntProc    link;
  xieIntProc    startup;
  xieIntProc    resume;
  xieIntProc    shutdown;
  xieIntProc    destroy;
} floVecRec, *floVecPtr;

/* ddxie client data manager entry points
 */
typedef struct _datavec {
  xieIntProc    input;
  xieIntProc    output;
  xieIntProc    query;
} dataVecRec, *dataVecPtr;

/* photo-element inFlo definition
 */
typedef struct _inflo {
  CARD8           bands;	/* number of bands this input will receive */
  CARD8           index;	/* inFlo index of this input		   */
  xieTypPhototag  srcTag;	/* phototag of source element              */
  struct _pedef  *srcDef;	/* pointer to source element definition    */
  struct _pedef  *ownDef;	/* pointer to this element's definition    */
  struct _inflo  *outChain;	/* chain of inFlos sharing the same output */
  formatRec       format[xieValMaxBands];   /* data format per input band  */
} inFloRec, *inFloPtr;

/* photo-element outFlo definition
 */
typedef struct _outflo {
  inFloPtr    outChain;		     /* chain  of inFlos receiving output  */
  bandMsk     active;		     /* bands that will supply client data */
  bandMsk     ready;		     /* bands that are holding client data */
  CARD8       bands;		     /* number of output bands	           */
  CARD8       pad;
  stripLstRec output[xieValMaxBands];/* holding place for element's output */
  formatRec   format[xieValMaxBands];/* data format per output band        */
} outFloRec, *outFloPtr;

/* photo-element definition structure
 */
typedef struct _pedef {
  struct _pedef  *flink;	/* foreword link in DAG-ordered peDef list */
  struct _pedef  *blink;	/* backword link in DAG-ordered peDef list */
  struct _pedef  *clink;        /* next element of the same class          */
  struct _pedef  *olink;        /* link to optimized element               */
  xieFlo         *elemRaw;      /* raw (swapped) client element		   */
  pointer	  elemPvt;	/* dixie element-private info		   */
  pointer	  techPvt;	/* dixie technique-private info		   */
  techVecPtr	  techVec;	/* technique vectors			   */
  ddPETexPtr	  peTex;	/* DDXIE execution context		   */
  inFloPtr	  inFloLst;	/* pointer to in-line array of input info  */
  CARD16	  inCnt;	/* number of inputs (inc. LUT & domain)    */
  xieTypPhototag  phototag;     /* phototag of this element                */
  struct {
    unsigned int import   :  1;	/* element performs import functions	   */
    unsigned int process  :  1;	/* element performs processing		   */
    unsigned int export   :  1;	/* element performs export functions	   */
    unsigned int getData  :  1;	/* element allows GetClientData		   */
    unsigned int putData  :  1;	/* element allows PutClientData		   */
    unsigned int modified :  1;	/* modified attributes or parameters       */
    unsigned int loop     :  1;	/* loop-detection flag                     */
    unsigned int reserved :  1;
  } flags;
  CARD8 swapUnits[xieValMaxBands]; /* size of client data aggregates       */
  diElemVecPtr	  diVec;	/* DIXIE entry point vector		   */
  ddElemVecRec	  ddVec;	/* DDXIE entry point vector (in-line)	   */
  outFloRec	  outFlo;	/* output attribute/connection info	   */
#if 0
  inFloRec	  inFlo[inCnt];	/* array of in-line input connection info  */
#endif
} peDefRec, *peDefPtr;

/* link-pair for managing a circular doubly linked list of peDefs
 */
typedef struct _pedlst {
    peDefPtr flink;
    peDefPtr blink; 
} pedLstRec, *pedLstPtr;

/* photoflo definition structure
 */
typedef struct _flodef {
  struct _flodef *flink;	/* foreword link in nameSpace (immed only) */
  struct _flodef *blink;	/* backword link in nameSpace (immed only) */
  ClientPtr	 runClient;	/* client that is running the flo	   */
  ClientPtr	 reqClient;	/* client that is making current request   */
  ClientPtr	*awakenPtr;	/* list of clients Awaiting flo done       */
  CARD32	 awakenCnt;	/* number of clients Awaiting flo done     */
  CARD32         ID;		/* instance-id within nameSpace            */
  CARD32         spaceID;	/* photospace-id or 0 (serverIDspace)      */
  photospacePtr  space;		/* photospacePtr or NULL		   */
  ddFloTexPtr	 floTex;	/* DDXIE execution context info		   */
  floVecPtr	 floVec;	/* DDXIE photoflo management entry points  */
  dataVecPtr	 dataVec;	/* DDXIE client data manager entry points  */
  ddSchedVecPtr	 schedVec;	/* DDXIE scheduler entry points		   */
  ddStripVecPtr	 stripVec;	/* DDXIE strip manager entry points	   */
  pedLstRec      defDAG;	/* DAG peList (flink=import, blink=export) */
  pedLstRec      optDAG;	/* optimized DAG peList			   */
  peDefPtr	*peArray;	/* pointer to in-line peDef pointer array  */
  CARD16         peCnt;		/* number of elements in peArray	   */
  struct {
    unsigned int active   :  1; /* execute requested (else inactive)       */
    unsigned int modified :  1; /* something changed since last execution  */
    unsigned int notify   :  1; /* send an event upon flo completion	   */
    unsigned int aborted  :  1; /* the client aborted execution		   */
    unsigned int reserved : 12;
  } flags;
  xieFloEvn	 event;		/* generic event packet                    */
  xieFloErr	 error;		/* generic error packet                    */
#if 0
  peDefPtr	 peArray[peCnt];/* array of peDef pointers                 */
#endif
} floDefRec, *floDefPtr;

/* link-pair for managing a circular doubly linked list of floDefs
 */
typedef struct _flolst {
    floDefPtr flink;
    floDefPtr blink; 
} floLstRec, *floLstPtr;

#endif /* end _XIEH_FLOSTR */
