/* $Xorg: macro.h,v 1.4 2001/02/09 02:04:23 xorgcvs Exp $ */
/**** module macro.h ****/
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

	macro.h -- XIE utility macros

	Robert NC Shelley -- AGE Logic, Inc.  March 1993

******************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/include/macro.h,v 1.10 2001/12/14 19:58:14 dawes Exp $ */

#ifndef _XIEH_MACRO
#define _XIEH_MACRO

#include <error.h>

/* DDXIE photoflo management interface
 */
#define ddShutdown(flo)   (flo->floVec ? (*flo->floVec->shutdown)(flo) : 0)
#define ddDestroy(flo)    (flo->floVec ? (*flo->floVec->destroy) (flo) : 0)
#define ddLink(flo)       (*flo->floVec->link)(flo)
#define ddResume(flo)     (*flo->floVec->resume)(flo)
#define ddStartup(flo)    (*flo->floVec->startup)(flo)

/* DDXIE client data management interface
 */
#define ddInput(flo,ped,band,data,bytes,final) \
  		(*flo->dataVec->input)(flo,ped,band,data,bytes,final)
#define ddOutput(flo,ped,band,maxLen,term) \
  		(*flo->dataVec->output)(flo,ped,band,maxLen,term)
#define ddQuery(flo,lst,im,ex) (*flo->dataVec->query)(flo,lst,im,ex)

/* List management macros
 */
#define ListInit(head) \
		(((lstPtr)(head))->flink = (lstPtr)(head), \
		 ((lstPtr)(head))->blink = (lstPtr)(head))

#define ListEnd(current,head) ((lstPtr)(current) == (lstPtr)(head))

#define ListEmpty(head) (((lstPtr)(head))->flink == (lstPtr)(head))

#define InsertMember(new,prev) \
		{lstPtr ptr = (lstPtr)(prev); \
		   ((lstPtr)(new))->flink = ptr->flink; \
		   ((lstPtr)(new))->blink = ptr; ptr->flink = (lstPtr)(new); \
		   ((lstPtr)(new))->flink->blink = (lstPtr)(new);}

#define RemoveMember(ptr,old) \
		(ptr=old, \
		 ((lstPtr)(ptr))->blink->flink = ((lstPtr)(ptr))->flink, \
		 ((lstPtr)(ptr))->flink->blink = ((lstPtr)(ptr))->blink)


/* return amount that should be added to 'len' to make it modulo 'pad'
 */
#define Align(len,pad) (((pad)-(len)%(pad))%(pad))


/* swap a pair of pointers
 */
#define SwapPtr(p1,p2,pt) \
        	(*((pointer *)&(pt)) = (pointer )(p1), \
		 *((pointer *)&(p1)) = (pointer )(p2), \
		 *((pointer *)&(p2)) = (pointer )(pt))


/* compute the minimum number of bits ('depth') required to represent 'levels'
 */
#define SetDepthFromLevels(levels,depth) \
		if(levels > 2) { CARD32 _i = levels; \
		  for(depth = 0; (_i >>= 1); ++depth); \
		  if(((1 << depth) - 1) & levels) ++depth; } \
		else depth = levels ? 1 : 32

/* event convenience macros
 */
#define	SendElementEvent(flo,ped,code) \
		(flo->event.src   = ped->phototag, \
		 flo->event.type  = ped->elemRaw->elemType, \
		 flo->event.event = code, \
		 SendFloEvent(flo))
#define SendColorAllocEvent(flo,ped,clst,tn,td) \
		{((xieColorAllocEvn *)&flo->event)->colorList = clst; \
		 ((xieColorAllocEvn *)&flo->event)->colorAllocTechnique = tn; \
		 ((xieColorAllocEvn *)&flo->event)->data = td; \
		 SendElementEvent(flo,ped,xieEvnNoColorAlloc);}
#define SendDecodeNotifyEvent(flo,ped,b,tn,dw,dh,abt) \
		{((xieDecodeNotifyEvn *)&flo->event)->bandNumber = b; \
		 ((xieDecodeNotifyEvn *)&flo->event)->decodeTechnique = tn; \
		 ((xieDecodeNotifyEvn *)&flo->event)->width   = dw; \
		 ((xieDecodeNotifyEvn *)&flo->event)->height  = dh; \
		 ((xieDecodeNotifyEvn *)&flo->event)->aborted = abt; \
		 SendElementEvent(flo,ped,xieEvnNoDecodeNotify);}
#define SendExportAvailableEvent(flo,ped,b,d0,d1,d2) \
		{((xieExportAvailableEvn *)&flo->event)->bandNumber = b; \
		 ((xieExportAvailableEvn *)&flo->event)->data0 = d0; \
		 ((xieExportAvailableEvn *)&flo->event)->data1 = d1; \
		 ((xieExportAvailableEvn *)&flo->event)->data2 = d2; \
		 SendElementEvent(flo,ped,xieEvnNoExportAvailable);}
#define SendImportObscuredEvent(flo,ped,win,ex,ey,ew,eh) \
		{((xieImportObscuredEvn *)&flo->event)->window = win; \
		 ((xieImportObscuredEvn *)&flo->event)->x      = ex; \
		 ((xieImportObscuredEvn *)&flo->event)->y      = ey; \
		 ((xieImportObscuredEvn *)&flo->event)->width  = ew; \
		 ((xieImportObscuredEvn *)&flo->event)->height = eh; \
		 SendElementEvent(flo,ped,xieEvnNoImportObscured);}


#define	ExecProc(client,opcode) \
		((client_table[client->index].proc_table[opcode])(client))
#define	ExecSProc(client,opcode) \
		((client_table[client->index].sproc_table[opcode])(client))

#define CallProc(client) (ExecProc(client,stuff->opcode))
#define CallSProc(client) (ExecSProc(client,stuff->opcode))

#define MakeElement(flo,tag,pe) \
		((MakeTable[pe->elemType])(flo,tag,pe))
	
#define ELEMENT(type) \
	register type *raw, *stuff = (type *)pe

#define ELEMENT_SIZE_MATCH(type) \
		if(stuff->elemLength != sizeof(type)>>2) \
		  FloLengthError(flo,tag,stuff->elemType, return(NULL))

#define ELEMENT_AT_LEAST_SIZE(type) \
		if(stuff->elemLength < sizeof(type)>>2) \
		  FloLengthError(flo,tag,stuff->elemType, return(NULL))

#define ELEMENT_NEEDS_1_INPUT(input) \
		if(!(stuff->input)) \
		  FloSourceError(flo,tag,stuff->elemType, return(NULL))

#define ELEMENT_NEEDS_2_INPUTS(input1,input2) \
		if(!(stuff->input1) || !(stuff->input2)) \
		  FloSourceError(flo,tag,stuff->elemType, return(NULL))


#define ELEMENT_NEEDS_3_INPUTS(input1,input2,input3) \
		if(!(stuff->input1) || !(stuff->input2) || !(stuff->input3)) \
		  FloSourceError(flo,tag,stuff->elemType, return(NULL))

/*
 * if XIE is built as a module, it shouldn't call libc functions.
 * The following macros should wrap all calls in XIE
 */
#ifndef XFree86LOADER
#include <stdio.h>
#include <math.h>
#else
#include "xf86_ansic.h"
#endif

#endif /* end _XIEH_MACRO */
