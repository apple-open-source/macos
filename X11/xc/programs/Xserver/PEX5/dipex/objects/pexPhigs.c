/* $Xorg: pexPhigs.c,v 1.4 2001/02/09 02:04:14 xorgcvs Exp $ */

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
/* $XFree86: xc/programs/Xserver/PEX5/dipex/objects/pexPhigs.c,v 3.8 2001/12/14 19:57:44 dawes Exp $ */


/*++
 *	PEXCreatePhigsWks
 *	dipexFreePhigsWks
 *	PEXFreePhigsWks
 *	PEXGetWksInfo
 *	PEXGetDynamics
 *	PEXGetViewRep
 *	PEXRedrawAllStructures
 *	PEXUpdateWorkstations
 *	PEXExecuteDeferredActions
 *	PEXSetViewPriority
 *	PEXSetDisplayUpdateMode
 *	PEXMapDCtoWC
 *	PEXSetViewRep
 *	PEXSetWksWindow
 *	PEXSetWksViewport
 *	PEXSetHlhsrMode
 *	PEXPostStructure
 *	PEXUnpostStructure
 *	PEXUnpostAllStructures
 *	PEXGetWksPostings
 *	PEXRedrawClipRegion
 --*/

#include "X.h"
#include "Xproto.h"
#include "PEX.h"
#include "pexError.h"
#include "pex_site.h"
#include "ddpex4.h"
#include "pexLookup.h"
#include "pexos.h"


extern LUTAddWksXref();
extern void LostXResource();

static void RemoveWksFromDrawableList();

#ifdef min
#undef min
#endif
 
#ifdef max
#undef max
#endif

#define AddLut(lut_id, wks_lut_entry) \
    if (! ((lut) = (diLUTHandle) LookupIDByType ((lut_id), PEXLutType)) ) { \
	err = PEX_ERROR_CODE(PEXLookupTableError); \
	xfree((pointer)pw);\
	PEX_ERR_EXIT(err,(lut_id),cntxtPtr); }\
    wks_lut_entry = lut;

#define AddNs(ns_id, wks_ns_entry) \
    if (! (ns = (diNSHandle) LookupIDByType ((ns_id), PEXNameType)) ) { \
	err = PEX_ERROR_CODE(PEXNameSetError); \
	xfree((pointer)pw);\
	PEX_ERR_EXIT(err,(ns_id),cntxtPtr); } \
    wks_ns_entry = ns;



/*++	PEXCreatePhigsWks
 *
 --*/
ErrorCode
PEXCreatePhigsWks (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexCreatePhigsWksReq    *strmPtr;
{
    ErrorCode	err = Success;
    dipexPhigsWks *pw = 0;
    dipexWksDrawable *wks_draw = 0;
    dipexWksDrawableLink *plink = 0;
    ddWksInit tables;
    diLUTHandle lut = 0;
    diNSHandle ns = 0;
    DrawablePtr pdraw = 0;

    if (!LegalNewID(strmPtr->wks, cntxtPtr->client))
	PEX_ERR_EXIT(BadIDChoice,strmPtr->wks,cntxtPtr);

    LU_DRAWABLE(strmPtr->drawable, pdraw);

    pw = (dipexPhigsWks *) xalloc ((unsigned long)(sizeof(dipexPhigsWks)));
    if (!pw) {
	PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
    }
    pw->did = strmPtr->drawable;
    pw->dd_data.id = strmPtr->wks;
    pw->dd_data.deviceData = NULL;

    tables.pDrawable = pdraw;
    tables.drawableId = strmPtr->drawable;

    /* Line Bundle Table */
    AddLut (strmPtr->lineBundle, tables.pLineLUT);

    /* Marker Bundle Table */
    AddLut (strmPtr->markerBundle, tables.pMarkerLUT);

    /* Text Bundle Table */
    AddLut (strmPtr->textBundle, tables.pTextLUT);

    /* Interior Bundle Table */
    AddLut (strmPtr->interiorBundle, tables.pIntLUT);

    /* Edge Bundle Table */
    AddLut (strmPtr->edgeBundle, tables.pEdgeLUT);

    /* Colour Bundle Table */
    AddLut (strmPtr->colourTable, tables.pColourLUT);

    /* Light Table */
    AddLut (strmPtr->lightTable, tables.pLightLUT);

    /* Colour Bundle Table */
    /* Pattern Bundle Table */
    AddLut (strmPtr->patternTable, tables.pPatternLUT);

    /* Text Font Bundle Table */
    AddLut (strmPtr->textFontTable, tables.pFontLUT);

    /* Depth Cue Bundle Table */
    AddLut (strmPtr->depthCueTable, tables.pDepthCueLUT);

    /* Colour Approx Bundle Table */
    AddLut (strmPtr->colourApproxTable, tables.pColourAppLUT);

    /*	Namesets */
    AddNs ( strmPtr->highlightIncl, tables.pHighInclSet);
    AddNs ( strmPtr->highlightExcl, tables.pHighExclSet);
    AddNs ( strmPtr->invisIncl, tables.pInvisInclSet);
    AddNs ( strmPtr->invisExcl, tables.pInvisExclSet);

    /* Double buffering mode */
    tables.bufferMode = strmPtr->bufferMode;

    /* workstation-drawable cross-references */
    if (!LegalNewID(strmPtr->drawable, cntxtPtr->client)) {

	wks_draw =
	    (dipexWksDrawable *) xalloc ((unsigned long)(sizeof(dipexWksDrawable)
						+ sizeof(dipexWksDrawableLink)));
	if (!wks_draw) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
	wks_draw->id = strmPtr->drawable;
	wks_draw->x_drawable = pdraw;
	wks_draw->wks_list = (dipexWksDrawableLink *)(wks_draw +1);
	wks_draw->wks_list->wks = pw;
	wks_draw->wks_list->wksid = strmPtr->wks;
	wks_draw->wks_list->next = 0;
	if (!( AddResource(   strmPtr->drawable, PEXWksDrawableType,
				    (pointer)wks_draw))) {
	    PEX_ERR_EXIT(BadAlloc,0,cntxtPtr); }
    } else {
	plink = (dipexWksDrawableLink *) 
			xalloc ((unsigned long)(sizeof(dipexWksDrawableLink)));
	if (!plink) {
	    xfree((pointer)pw);
	    PEX_ERR_EXIT(BadAlloc,0,cntxtPtr); }
	plink->next = wks_draw->wks_list;
	plink->wksid = strmPtr->wks;
	wks_draw->wks_list = plink;
    }


    /* ddpex create */
    err = CreatePhigsWks (&tables, &(pw->dd_data));
    if (err) {
	RemoveWksFromDrawableList(strmPtr->wks, wks_draw);
	xfree((pointer)pw);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    };

    ADDRESOURCE(strmPtr->wks, PEXWksType, pw);

    return( err );

} /* end-PEXCreatePhigsWks() */

/*++	dipexFreePhigsWks
 *
 --*/

static void
RemoveWksFromDrawableList(wksid,drawlist)
pexPhigsWks	    wksid;
dipexWksDrawable    *drawlist;
{
    dipexWksDrawableLink *plink = drawlist->wks_list, *prev = 0;

    while (plink) {
	if (plink->wksid == wksid) {
	    if (prev) {
		prev->next = plink->next;
		xfree((pointer)plink);
	    } else { drawlist->wks_list = plink->next; }
	    plink = 0;
	} else {
	    prev = plink;
	    plink = plink->next;
	}
    }

    if (! (drawlist->wks_list) )  {
	/* don't do a free--weird side effects in FreeResource
	   will cause all types associated with this id to be freed */
	drawlist->id = PEXAlreadyFreed;
    }

}
/*++	dipexFreePhigsWks
 *
 --*/
ErrorCode
dipexFreePhigsWks (pw, id)
dipexPhigsWks	*pw;
pexPhigsWks	id;
{
    ErrorCode	err = Success;
    extern ddpex4rtn FreePhigsWks();
    void RemoveWksFromDrawableList();
    dipexWksDrawable *wks_draw = 0;

    if ((wks_draw =
	(dipexWksDrawable *) LookupIDByType(pw->did,PEXWksDrawableType))) {
	    RemoveWksFromDrawableList(id,wks_draw);
	}

    err = FreePhigsWks((diWKSHandle)pw,id);

    return( err );

} /* dipexFreePhigsWks() */

/*++	PEXFreePhigsWks
 --*/
ErrorCode
PEXFreePhigsWks (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexFreePhigsWksReq      *strmPtr;
{
    ErrorCode err = Success; 
    dipexPhigsWks *pw = 0;

    if ((strmPtr == NULL) || (strmPtr->id == 0)) {
	err = PEX_ERROR_CODE(PEXPhigsWksError);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    }

    LU_PHIGSWKS (strmPtr->id, pw);

    FreeResource(strmPtr->id, RT_NONE);

    return( err );

} /* end-PEXFreePhigsWks() */

/*++	PEXGetWksInfo
 --*/
ErrorCode
PEXGetWksInfo( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexGetWksInfoReq    	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;
    extern ddBuffer *pPEXBuffer;
    CARD32 numValues;

    LU_PHIGSWKS(strmPtr->wks, pw);

    SETUP_INQ(pexGetWksInfoReply);

    err = InquireWksInfo( (diWKSHandle)pw, strmPtr->itemMask, &numValues, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetWksInfoReply );
	WritePEXBufferReply(pexGetWksInfoReply);
    }
    return( err );

} /* end-PEXGetWksInfo() */

/*++	PEXGetDynamics
 --*/
ErrorCode
PEXGetDynamics( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexGetDynamicsReq    	*strmPtr;
{
    ErrorCode err = Success;
    DrawablePtr pdraw = 0;
    extern ddBuffer *pPEXBuffer;
    pexGetDynamicsReply *reply = (pexGetDynamicsReply *)(pPEXBuffer->pHead);

    LU_DRAWABLE(strmPtr->drawable, pdraw);
    SETUP_INQ(pexGetDynamicsReply);

    err = InquireWksDynamics (pdraw, (ddWksDynamics *)&(reply->viewRep));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    reply->length = 0;
    WritePEXReplyToClient(cntxtPtr, strmPtr, sizeof(pexGetDynamicsReply), reply);
    return( err );

} /* end-PEXGetDynamics() */

/*++	PEXGetViewRep
 --*/

ErrorCode
PEXGetViewRep( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexGetViewRepReq    	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;
    extern ddBuffer *pPEXBuffer;
    pexGetViewRepReply *reply = (pexGetViewRepReply *)(pPEXBuffer->pHead);
    pexViewRep *View1, *View2;
    unsigned long size = 0;

    LU_PHIGSWKS(strmPtr->wks, pw);

    SETUP_INQ(pexGetViewRepReply);

    if (pPEXBuffer->bufSize < size) {
	err = puBuffRealloc(pPEXBuffer, size);
	if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
	reply = (pexGetViewRepReply *)(pPEXBuffer->pHead);
    }

    View1 = (pexViewRep *)(pPEXBuffer->pBuf);
    View2 = View1 + 1;
    err = InquireViewRep(   (diWKSHandle)pw, strmPtr->index,
			    &(reply->viewUpdate), View1, View2);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    size = sizeof(pexViewRep);
    reply->length = 2 * LWORDS(size);
    size = (sizeof(CARD32) * reply->length) + sizeof(pexGetViewRepReply);
    WritePEXReplyToClient (cntxtPtr, strmPtr, size, reply);
    return( err );

} /* end-PEXGetViewRep() */

/*++	PEXRedrawAllStructures
 --*/
ErrorCode
PEXRedrawAllStructures( cntxtPtr, strmPtr )
pexContext      		*cntxtPtr;
pexRedrawAllStructuresReq	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS(strmPtr->id, pw);

    err = RedrawStructures ((diWKSHandle)pw);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXRedrawAllStructures() */

/*++	PEXUpdateWorkstation
 --*/
ErrorCode
PEXUpdateWorkstation (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexUpdateWorkstationReq	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS(strmPtr->id, pw);

    err = UpdateWks((diWKSHandle)pw);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXUpdateWorkstation() */

/*++	PEXExecuteDeferredActions
 --*/

ErrorCode
PEXExecuteDeferredActions( cntxtPtr, strmPtr )
pexContext      		*cntxtPtr;
pexExecuteDeferredActionsReq    *strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS(strmPtr->id, pw);

    err = ExecuteDeferred ((diWKSHandle)pw);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXExecuteDeferredActions() */

/*++	PEXSetViewPriority
 --*/
ErrorCode
PEXSetViewPriority( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexSetViewPriorityReq   *strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS(strmPtr->wks, pw);

    err = SetViewPriority(  (diWKSHandle)pw, strmPtr->index1, strmPtr->index2, 
			    strmPtr->priority);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXSetViewPriority() */

/*++	PEXSetDisplayUpdateMode
 --*/
ErrorCode
PEXSetDisplayUpdateMode( cntxtPtr, strmPtr )
pexContext      		*cntxtPtr;
pexSetDisplayUpdateModeReq    	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS(strmPtr->wks, pw);

    err = SetDisplayUpdateMode ((diWKSHandle)pw, strmPtr->displayUpdate);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXSetDisplayUpdateMode() */

/*++	PEXMapDCtoWC
 --*/
ErrorCode
PEXMapDCtoWC( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexMapDCtoWCReq    	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;
    extern ddBuffer *pPEXBuffer;
    pexMapDCtoWCReply *reply;
    unsigned long reply_size;

    LU_PHIGSWKS(strmPtr->wks, pw);

    SETUP_INQ(pexMapDCtoWCReply);

    pPEXBuffer->dataSize = strmPtr->numCoords * sizeof(pexCoord3D);
    reply_size = pPEXBuffer->dataSize + sizeof(pexMapDCtoWCReply);
    if (pPEXBuffer->bufSize < reply_size)
	if (err = puBuffRealloc(pPEXBuffer, reply_size)) return (err);

    reply = (pexMapDCtoWCReply *)(pPEXBuffer->pHead);

    err = MapDcWc(  (diWKSHandle)pw, strmPtr->numCoords, (pexDeviceCoord *)(strmPtr+1),
		    &(reply->numCoords), (pexCoord3D *)(pPEXBuffer->pBuf),
		    &(reply->viewIndex));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    reply->length = LWORDS(pPEXBuffer->dataSize);
    WritePEXBufferReply(pexMapDCtoWCReply);
    return (err);

} /* end-PEXMapDCtoWC() */

/*++	PEXMapWCtoDC
 --*/
ErrorCode
PEXMapWCtoDC( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexMapWCtoDCReq    	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;
    extern ddBuffer *pPEXBuffer;
    pexMapWCtoDCReply *reply;
    unsigned long reply_size;

    LU_PHIGSWKS(strmPtr->wks, pw);

    SETUP_INQ(pexMapWCtoDCReply);

    pPEXBuffer->dataSize = strmPtr->numCoords * sizeof(pexDeviceCoord);
    reply_size = pPEXBuffer->dataSize + sizeof(pexMapWCtoDCReply);
    if (pPEXBuffer->bufSize < reply_size)
	if (err = puBuffRealloc(pPEXBuffer, reply_size)) return (err);

    reply = (pexMapWCtoDCReply *)(pPEXBuffer->pHead);

    err = MapWcDc(  (diWKSHandle)pw, strmPtr->numCoords,
		    (pexCoord3D *)(strmPtr + 1), strmPtr->index,
		    &(reply->numCoords), (pexDeviceCoord *)(pPEXBuffer->pBuf));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    reply->length = LWORDS(pPEXBuffer->dataSize);
    WritePEXBufferReply(pexMapWCtoDCReply);
    return( err );

} /* end-PEXMapWCtoDC() */

/*++	PEXSetViewRep
 --*/
ErrorCode
PEXSetViewRep (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexSetViewRepReq    	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS(strmPtr->wks, pw);
    CHECK_FP_FORMAT (strmPtr->fpFormat);

    err = SetViewRep ((diWKSHandle)pw, &(strmPtr->viewRep));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXSetViewRep() */

/*++	PEXSetWksWindow
 --*/

ErrorCode
PEXSetWksWindow( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexSetWksWindowReq    	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS(strmPtr->wks, pw);
    CHECK_FP_FORMAT (strmPtr->fpFormat);

    return (SetWksWindow((diWKSHandle)pw, &(strmPtr->npcSubvolume)));

} /* end-PEXSetWksWindow() */

/*++	PEXSetWksViewport
 --*/

ErrorCode
PEXSetWksViewport( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexSetWksViewportReq    *strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS (strmPtr->wks, pw);
    CHECK_FP_FORMAT (strmPtr->fpFormat);

    return(SetWksViewport ((diWKSHandle)pw, &(strmPtr->viewport)));

} /* end-PEXSetWksViewport() */

/*++	PEXSetHlhsrMode
 --*/

ErrorCode
PEXSetHlhsrMode( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexSetHlhsrModeReq    	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS( strmPtr->wks, pw);

    return (SetHlhsrMode((diWKSHandle)pw, strmPtr->mode));

} /* end-PEXSetHlhsrMode() */


/*++	PEXSetWksBufferMode
 --*/
ErrorCode
PEXSetWksBufferMode (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexSetWksBufferModeReq	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS( strmPtr->wks, pw);

    return (SetBufferMode((diWKSHandle)pw, strmPtr->bufferMode));
}


/*++	PEXPostStructure
 --*/
ErrorCode
PEXPostStructure (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexPostStructureReq	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;
    diStructHandle pstr = 0;

    LU_PHIGSWKS (strmPtr->wks, pw);

    LU_STRUCTURE(strmPtr->sid, pstr);

    return (PostStructure ((diWKSHandle)pw, pstr, strmPtr->priority));

} /* end-PEXPostStructure() */

/*++	PEXUnpostStructure
 --*/

ErrorCode
PEXUnpostStructure( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexUnpostStructureReq   *strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;
    diStructHandle pstr = 0;

    LU_PHIGSWKS (strmPtr->wks, pw);

    LU_STRUCTURE(strmPtr->sid, pstr);

    return (UnpostStructure ((diWKSHandle)pw, pstr));

} /* end-PEXUnpostStructure() */

/*++	PEXUnpostAllStructures
 --*/

ErrorCode
PEXUnpostAllStructures( cntxtPtr, strmPtr )
pexContext     			*cntxtPtr;
pexUnpostAllStructuresReq    	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS (strmPtr->id, pw);

    return (UnpostAllStructures ((diWKSHandle)pw));

} /* end-PEXUnpostAllStructures() */
ErrorCode
PEXGetWksPostings(  cntxtPtr, strmPtr )
pexContext		*cntxtPtr;
pexGetWksPostingsReq    *strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;
    extern ddBuffer *pPEXBuffer;

    LU_STRUCTURE(strmPtr->id, pstr);

    SETUP_INQ(pexGetWksPostingsReply);

    err = InquireWksPostings (pstr, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetWksPostingsReply);
	WritePEXBufferReply(pexGetWksPostingsReply);
    }

    return( err );

} /* end-PEXGetWksPostings() */

/*++	PEXRedrawClipRegion
 --*/
ErrorCode
PEXRedrawClipRegion( cntxtPtr, strmPtr )
pexContext		*cntxtPtr;
pexRedrawClipRegionReq	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;

    LU_PHIGSWKS (strmPtr->wks, pw);

    return (RedrawClipRegion(	(diWKSHandle)pw, strmPtr->numRects,
				(ddDeviceRect *)(strmPtr+1)));
}

/*++	FreeWksDrawable
 --*/
#define LostDrawable(w) \
   LostXResource((diResourceHandle)(w),WORKSTATION_RESOURCE,X_DRAWABLE_RESOURCE)

FreeWksDrawable(ptr, id)
dipexWksDrawable *ptr;
Drawable id;
{
    dipexWksDrawableLink *plink = 0, *pnext = 0;

    if (!ptr) return (Success);

    if (ptr->id == PEXAlreadyFreed) {
	xfree((pointer)ptr);
	return(Success); }

    if (ptr->id != id) {
	ErrorF( "Corrupted wks-drawable list: %d %d", id, ptr->id); } else
    if (ptr->wks_list) {
	LostDrawable(ptr->wks_list->wks);

	/* first link is allocated in the same chunk as the header */
	plink = ptr->wks_list->next;
	ptr->wks_list = 0;
	while (plink) {
	    pnext = plink->next;
	    LostDrawable(plink->wks);
	    xfree((pointer)plink);
	    plink = pnext;
	} }

    xfree((pointer)ptr);

    return (Success);
}
/*++
 *
 *	End of File
 *
 --*/
