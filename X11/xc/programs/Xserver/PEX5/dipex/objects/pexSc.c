/* $Xorg: pexSc.c,v 1.4 2001/02/09 02:04:14 xorgcvs Exp $ */

/***********************************************************

Copyright 1989, 1990, 1991, 1998  The Open Group

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

Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution of 
the software without specific, written prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $XFree86: xc/programs/Xserver/PEX5/dipex/objects/pexSc.c,v 3.7 2001/12/14 19:57:44 dawes Exp $ */


/*++
 *	PEXCreateSearchContext
 *	PEXCopySearchContext
 *	PEXFreeSearchContext
 *	PEXGetSearchContext
 *	PEXChangeSearchContext
 *	PEXSearchNetwork
 --*/

#include "X.h"
#include "Xproto.h"
#include "pexError.h"
#include "dipex.h"
#include "PEXproto.h"
#include "pex_site.h"
#include "ddpex4.h"
#include "pexLookup.h"
#include "pexUtils.h"
#include "pexExtract.h"
#include "pexos.h"

#ifdef min
#undef min
#endif
 
#ifdef max
#undef max
#endif


#define SC_NS_LIMIT  20				/* arbitrary value */

#define CHK_PEX_BUF(SIZE,INCR,REPLY,TYPE,PTR) {\
    (SIZE)+=(INCR); \
      if (pPEXBuffer->bufSize < (SIZE)) { \
	ErrorCode err = Success; \
	int offset = (int)(((unsigned char *)(PTR)) - ((unsigned char *)(pPEXBuffer->pHead))); \
	err = puBuffRealloc(pPEXBuffer,(ddULONG)(SIZE)); \
	if (err) PEX_ERR_EXIT(err,0,cntxtPtr); \
	(REPLY) = (TYPE *)(pPEXBuffer->pHead); \
	(PTR) = (unsigned char *)(pPEXBuffer->pHead + offset); } \
}

static ErrorCode
AddToNSPair( incl, excl, pair )
diNSHandle incl, excl;
listofNSPair *pair;
{
    ddNSPair *ptr;
    if (pair->numPairs >= pair->maxPairs) {
	pair->maxPairs += SC_NS_LIMIT;
	pair->pPairs = (ddNSPair *)xrealloc(    (pointer)(pair->pPairs),
						(unsigned long)(pair->maxPairs
						    * sizeof(ddNSPair)));
	if (!pair->pPairs) return(BadAlloc);
    }
    ptr = pair->pPairs + (unsigned long)(pair->numPairs);
    ptr->incl = incl; ptr->excl = excl;
    pair->numPairs++;

    return( Success );
}


/*
    add things to a list which has number things in it and can
    fill in up to end before needing to molt; shoe sizes come in quanta of incr

    note arrays of pointers do not point to contiguous structures 
 */
static ErrorCode
diAddThingToArray( thing, array, number, end, incr)
unsigned long	thing;
unsigned long	**array;
int		*number;
unsigned long	*end;
int		incr;
{
    int i = *number;
    unsigned long *ptr = *array;

    /* check to see if it's in the array already */
    if ( i ) {
	for ( ; i>0; i--, ptr++ ) if (*ptr == thing)
	    return(BadIDChoice);
    };

    if ((!array) ||
	((int)(*array) + *number > (int)end)) {	/*  need more room in array */
	unsigned long **bigger_array;
	bigger_array = 
	    (unsigned long **)xrealloc((pointer)array, 
					(unsigned long)(sizeof(unsigned long) *
					  ((int)end - (int)*array + incr)));
	if (!bigger_array) return(BadAlloc);
	end += incr;
	*array = *bigger_array;
    };

    *ptr = thing;
    *number++;
    return( Success );
}

static ErrorCode
diAddThingToList( thing, list, number, end, incr)
unsigned long	thing;
unsigned long	*list;
int		*number;
unsigned long	*end;
int		incr;
{
    int i = *number;
    unsigned long *ptr = list;

    /* check to see if it's in the list already */
    if ( i ) {
	for ( ; i>0; i--, ptr++ ) if (*ptr == thing)
	    return(BadIDChoice);
    };

    if ((!list) ||
	((int)(list) + *number > (int)end)) {	/*  need more room in list */
	unsigned long *bigger_list;
	bigger_list = 
		(unsigned long *)xrealloc((pointer)list, 
					 (unsigned long)(sizeof(unsigned long) *
					     ((int)end - (int)list + incr)));
	if (!bigger_list) return(BadAlloc);
	end += incr;
	list = bigger_list;
    };

    *ptr = thing;
    *number++;
    return( Success );
}


static ErrorCode
UpdateSearchContext (cntxtPtr, psc, itemMask, ptr)
pexContext	*cntxtPtr;
ddSCStr		*psc;
pexBitmask	itemMask;
unsigned char	*ptr;
{
    ErrorCode err = Success;

    if (itemMask & PEXSCPosition) {
	EXTRACT_COORD3D (&(psc->position), ptr);
    };

    if (itemMask & PEXSCDistance) {
	EXTRACT_FLOAT (psc->distance, ptr);
    };

    if (itemMask & PEXSCCeiling) {
	EXTRACT_CARD16_FROM_4B (psc->ceiling, ptr);
    }

    if (itemMask & PEXSCModelClipFlag) {
	EXTRACT_CARD8_FROM_4B (psc->modelClipFlag, ptr);
    }

    if (itemMask & PEXSCStartPath) {
	pexElementRef *per;
	diStructHandle sh, *psh;
	CARD32 i, numRefs = 0;
	extern ddpex4rtn ValidateStructurePath();

	EXTRACT_CARD32 (numRefs, ptr);
	for (i=0, per = (pexElementRef *)ptr; i<numRefs; i++, per++) {
		LU_STRUCTURE(per->structure,sh);
		psh = (diStructHandle *)&(per->structure);
		*psh = sh;
	}
	if (psc->startPath) puDeleteList(psc->startPath);
        psc->startPath = puCreateList(DD_ELEMENT_REF);
	if (!(psc->startPath)) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
	puAddToList((ddPointer)ptr, numRefs, psc->startPath);
	err = ValidateStructurePath(psc->startPath);
	if (err != Success) PEX_ERR_EXIT(err,0,cntxtPtr);
	ptr = (unsigned char *)per;
    };

    if (itemMask & PEXSCNormalList) {
	unsigned long i, len;
	pexNameSetPair *pnsp=0;
	diNSHandle pi, pe;
	EXTRACT_CARD32 (len, ptr);
	for (i=0, pnsp = (pexNameSetPair *)ptr; i<len; i++, pnsp++) {
	    LU_NAMESET (pnsp->incl, pi);
	    LU_NAMESET (pnsp->excl, pe);
	    err = AddToNSPair(pi, pe, &(psc->normal));
	    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
	};

	ptr = (unsigned char *)pnsp;
    };

    if (itemMask & PEXSCInvertedList) {
	unsigned long i, len;
	pexNameSetPair *pnsp=0;
	diNSHandle pi, pe;
	EXTRACT_CARD32 (len, ptr);
	for (i=0, pnsp = (pexNameSetPair *)ptr; i<len; i++, pnsp++) {
	    LU_NAMESET (pnsp->incl, pi);
	    LU_NAMESET (pnsp->excl, pe);
	    err = AddToNSPair(pi, pe, &(psc->inverted));
	    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
	};

	ptr = (unsigned char *)pnsp;
    };

    return( err );
}


/*++	PEXCreateSearchContext
 --*/
ErrorCode
PEXCreateSearchContext (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexCreateSearchContextReq *strmPtr;
{
    ErrorCode err = Success;
    ErrorCode freeSearchContext();
    ddSCStr *psc;
    unsigned char *ptr;

    if (!LegalNewID(strmPtr->sc, cntxtPtr->client))
	PEX_ERR_EXIT(BadIDChoice,strmPtr->sc,cntxtPtr);

    CHECK_FP_FORMAT (strmPtr->fpFormat);

    psc = (ddSCStr *)xalloc((unsigned long)sizeof(ddSCStr));
    if (!psc) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
    psc->id = strmPtr->sc;
    psc->normal.numPairs=0;
    psc->normal.maxPairs=SC_NS_LIMIT;
    psc->normal.pPairs =
		(ddNSPair *)xalloc(psc->normal.maxPairs * sizeof(ddNSPair));
    psc->inverted.numPairs=0;
    psc->inverted.maxPairs=SC_NS_LIMIT;
    psc->inverted.pPairs =
		(ddNSPair *)xalloc(psc->inverted.maxPairs * sizeof(ddNSPair));

    psc->position.x = psc->position.y = psc->position.z = 0.0;
    psc->distance = 0.0;
    psc->ceiling = 1;
    psc->modelClipFlag = xFalse;

    if (strmPtr->itemMask & PEXSCStartPath)
	psc->startPath = 0;   /* list will be created in UpdateSearchContext */
    else
	psc->startPath = puCreateList(DD_ELEMENT_REF);

    ptr = (unsigned char *) (strmPtr + 1);


    err = UpdateSearchContext(cntxtPtr, psc, strmPtr->itemMask, ptr);
    if (err) {
	puDeleteList(psc->startPath);
	xfree((pointer)psc);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    }

    ADDRESOURCE(strmPtr->sc, PEXSearchType, psc);
    return( err );

} /* end-PEXCreateSearchContext() */

/*++	PEXCopySearchContext
 --*/
ErrorCode
PEXCopySearchContext (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexCopySearchContextReq *strmPtr;
{
    ErrorCode err = Success;
    ddSCStr *src, *dst;

    LU_SEARCHCONTEXT (strmPtr->src, src);
    LU_SEARCHCONTEXT (strmPtr->dst, dst);

    if (strmPtr->itemMask & PEXSCPosition) dst->position = src->position;

    if (strmPtr->itemMask & PEXSCDistance) dst->distance = src->distance;

    if (strmPtr->itemMask & PEXSCCeiling) dst->ceiling = src->ceiling;

    if (strmPtr->itemMask & PEXSCModelClipFlag)
	dst->modelClipFlag = src->modelClipFlag;

    if (strmPtr->itemMask & PEXSCStartPath) {
	puDeleteList (dst->startPath);
	dst->startPath = puCreateList(DD_ELEMENT_REF);
	if (!dst->startPath->pList) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
	puCopyList(src->startPath, dst->startPath);
    };


    if (strmPtr->itemMask & PEXSCNormalList) {
	xfree((pointer)(dst->normal.pPairs));
	dst->normal.pPairs =
	    (ddNSPair *) xalloc(   (unsigned long)(src->normal.maxPairs * 
						 sizeof(ddNSPair)));
	if (! dst->normal.pPairs) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);

	memcpy( (char *)(dst->normal.pPairs), (char *)(src->normal.pPairs), 
		(int)(src->normal.numPairs * sizeof(ddNSPair)));
	dst->normal.numPairs = src->normal.numPairs;
	dst->normal.maxPairs = src->normal.maxPairs;
    }


    if (strmPtr->itemMask & PEXSCInvertedList) {
	xfree((pointer)dst->inverted.pPairs);
	dst->inverted.pPairs =
	    (ddNSPair *)xalloc((unsigned long)(src->inverted.maxPairs*
						 sizeof(ddNSPair)));
	if (! dst->inverted.pPairs) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);

	memcpy( (char *)(dst->inverted.pPairs), (char *)(src->inverted.pPairs),
		(int)(src->inverted.numPairs * sizeof(ddNSPair)));
	dst->inverted.numPairs = src->inverted.numPairs;
	dst->inverted.maxPairs = src->inverted.maxPairs;
    }

    return( err );

} /* end-PEXCopySearchContext() */

ErrorCode
FreeSearchContext (ptr, id)
ddSCStr *ptr;
pexSC id;
{
    if (ptr->inverted.pPairs) xfree ((pointer)(ptr->inverted.pPairs));
    if (ptr->normal.pPairs) xfree ((pointer)(ptr->normal.pPairs));
    puDeleteList(ptr->startPath);

    xfree((pointer)ptr);

    return( Success );
}

/*++	PEXFreeSearchContext
 --*/
ErrorCode
PEXFreeSearchContext (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexFreeSearchContextReq *strmPtr;
{
    ErrorCode err = Success;
    ddSCStr *psc;

    if ((strmPtr == NULL) || (strmPtr->id == 0)) {
	err = PEX_ERROR_CODE(PEXSearchContextError);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    }

    LU_SEARCHCONTEXT (strmPtr->id, psc);

    FreeResource(strmPtr->id, RT_NONE);

    return( err );

} /* end-PEXFreeSearchContext() */

/*++	PEXGetSearchContext
 --*/
ErrorCode
PEXGetSearchContext( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexGetSearchContextReq  *strmPtr;
{
    ErrorCode err = Success;
    ddSCStr *psc;
    extern ddBuffer *pPEXBuffer;
    unsigned char *ptr = 0;
    pexGetSearchContextReply *reply
			    = (pexGetSearchContextReply *)(pPEXBuffer->pHead);
    int size = 0;

    LU_SEARCHCONTEXT (strmPtr->sc, psc);

    CHK_PEX_BUF(size, sizeof(pexGetSearchContextReply), reply,
		pexGetSearchContextReply, ptr);
    SETUP_INQ(pexGetSearchContextReply);
    ptr = pPEXBuffer->pBuf;

    if (strmPtr->itemMask & PEXSCPosition) {
	CHK_PEX_BUF(size, sizeof(pexCoord3D), reply, pexGetSearchContextReply,
		    ptr);
	PACK_COORD3D (&(psc->position), ptr);
    };

    if (strmPtr->itemMask & PEXSCDistance) {
	CHK_PEX_BUF(size, sizeof(PEXFLOAT), reply, pexGetSearchContextReply, ptr);
	PACK_FLOAT (psc->distance, ptr);
    };

    if (strmPtr->itemMask & PEXSCCeiling) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetSearchContextReply, ptr);
	PACK_CARD32 (psc->ceiling, ptr);
    }

    if (strmPtr->itemMask & PEXSCModelClipFlag) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetSearchContextReply, ptr);
	PACK_CARD32 (psc->modelClipFlag, ptr);
    }

    if (strmPtr->itemMask & PEXSCStartPath) {
	pexStructure sid = 0;
	unsigned long i;
	ddElementRef *per = (ddElementRef *)(psc->startPath->pList);

	CHK_PEX_BUF(size,
		    psc->startPath->numObj*sizeof(pexElementRef) +sizeof(CARD32),
		    reply, pexGetSearchContextReply,ptr);
	PACK_CARD32(psc->startPath->numObj,ptr);
	for (i=0; i<psc->startPath->numObj; i++, per++) {
	    sid = GetId(per->structure);
	    PACK_CARD32(sid, ptr);
	    PACK_CARD32(per->offset, ptr);
	}
    };

    if (strmPtr->itemMask & PEXSCNormalList) {
	int i;
	ddNSPair *src;
	pexNameSet *dst;
	CHK_PEX_BUF(size, psc->normal.numPairs * 2 * sizeof(pexNameSet) + 4,
		    reply, pexGetSearchContextReply, ptr);
	PACK_CARD32(psc->normal.numPairs, ptr);
	for (i=0, src = psc->normal.pPairs, dst = (pexNameSet *)ptr;
	     i<psc->normal.numPairs; i++, src++) {
		*dst++ = src->incl->id;
		*dst++ = src->excl->id; }
	ptr = (unsigned char *)dst;
    };

    if (strmPtr->itemMask & PEXSCInvertedList) {
	int i;
	ddNSPair *src;
	pexNameSet *dst;
	CHK_PEX_BUF(size, psc->inverted.numPairs * 2 * sizeof(pexNameSet) + 4,
		    reply, pexGetSearchContextReply, ptr);
	PACK_CARD32(psc->inverted.numPairs, ptr);
	for (i=0, src = psc->inverted.pPairs, dst = (pexNameSet *)ptr;
	     i<psc->inverted.numPairs; i++, src++) {
		*dst++ = src->incl->id;
		*dst++ = src->excl->id; }
	ptr = (unsigned char *)dst;
    };

    reply->length = size - sizeof(pexGetSearchContextReply);
    reply->length = LWORDS(reply->length);
    WritePEXReplyToClient(  cntxtPtr, strmPtr,
        sizeof (pexGetSearchContextReply) + sizeof(CARD32) * reply->length,
	reply);

    return( err );

} /* end-PEXGetSearchContext() */

/*++	PEXChangeSearchContext
 --*/
ErrorCode
PEXChangeSearchContext( cntxtPtr, strmPtr )
pexContext     			*cntxtPtr;
pexChangeSearchContextReq    	*strmPtr;
{
    ErrorCode err = Success;
    ddSCStr *psc;
    unsigned char *ptr = (unsigned char *) (strmPtr + 1);

    LU_SEARCHCONTEXT (strmPtr->sc, psc);

    CHECK_FP_FORMAT (strmPtr->fpFormat);

    err = UpdateSearchContext(cntxtPtr, psc, strmPtr->itemMask, ptr);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXChangeSearchContext() */

/*++	PEXSearchNetwork
 --*/
ErrorCode
PEXSearchNetwork( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexSearchNetworkReq    	*strmPtr;
{
    ErrorCode err = Success;
    ddSCStr *psc;
    extern ddBuffer *pPEXBuffer;
    CARD32 numItems;

    LU_SEARCHCONTEXT (strmPtr->id, psc);

    SETUP_INQ(pexSearchNetworkReply);

    err = SearchNetwork (psc, &numItems, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexSearchNetworkReply);
	reply->numItems = numItems;
	WritePEXBufferReply(pexSearchNetworkReply);
    }
    return( err );

} /* end-PEXSearchNetwork() */
/*++
 *
 *	End of File
 *
 --*/
