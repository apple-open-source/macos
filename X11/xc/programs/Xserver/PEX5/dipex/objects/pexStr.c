/* $Xorg: pexStr.c,v 1.4 2001/02/09 02:04:14 xorgcvs Exp $ */

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


/*++
 *	PEXCreateStructure
 *	PEXCopyStructure
 *	PEXDestroyStructures
 *	PEXGetStructureInfo
 *	PEXSetStructurePermission
 *	PEXGetElementInfo
 *	PEXGetStructuresInNetwork
 *	PEXGetAncestors
 *	PEXGetDescendants
 *	PEXFetchElements
 *	PEXSetEditingMode
 *	PEXSetElementPointer
 *	PEXSetElementPointerAtLabel
 *	PEXSetElementPointerAtPickID
 *	PEXElementSearch
 *	PEXStoreElements
 *	PEXDeleteElements
 *	PEXDeleteElementsToLabel
 *	PEXDeleteBetweenLabels
 *	PEXCopyElements
 *	PEXChangeStructureReferences
 --*/
#include "X.h"
#include "PEX.h"
#include "pexError.h"
#include "pex_site.h"
#include "pexLookup.h"
#include "ddpex4.h"

#ifdef min
#undef min
#endif
 
#ifdef max
#undef max
#endif


/*++	PEXCreateStructure
 --*/
ErrorCode
PEXCreateStructure (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexCreateStructureReq   *strmPtr;
{
    ErrorCode err = Success;
    ErrorCode DeleteStructure ();
    diStructHandle sh;

    if (!LegalNewID(strmPtr->id, cntxtPtr->client))
	PEX_ERR_EXIT(BadIDChoice,strmPtr->id,cntxtPtr);

    sh = (diStructHandle)xalloc((unsigned long)sizeof(ddStructResource));
    if (!sh) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);

    sh->id = strmPtr->id;
    err = CreateStructure(sh);
    if (err) {
	xfree((pointer)sh);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    }

    ADDRESOURCE(strmPtr->id, PEXStructType, sh);
    return( err );

} /* end-PEXCreateStructure() */

/*++	PEXCopyStructure
 --*/
ErrorCode
PEXCopyStructure (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexCopyStructureReq     *strmPtr;
{
    ErrorCode err = Success;
    diStructHandle sh1, sh2;

    LU_STRUCTURE (strmPtr->src, sh1);
    LU_STRUCTURE (strmPtr->dst, sh2);

    err = CopyStructure (sh1, sh2);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXCopyStructure() */

/*++	PEXDeleteStructures
 --*/
PEXDestroyStructures (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexDestroyStructuresReq  *strmPtr;
{
    ErrorCode err = Success;
    diStructHandle ps = 0;
    int i;
    pexStructure *pid;

    for ( i = 0, pid = (pexStructure *)(strmPtr+1); 
	  (i < strmPtr->numStructures) && (err == Success); i++, pid++) {
	LU_STRUCTURE(*pid, ps);
	FreeResource (*pid, RT_NONE);
    }

    return (err);

} /* end-PEXDestroyStructures() */

/*++	PEXGetStructureInfo
 --*/
ErrorCode
PEXGetStructureInfo( cntxtPtr, strmPtr )
pexContext     		*cntxtPtr;
pexGetStructureInfoReq  *strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;
    extern ddBuffer *pPEXBuffer;
    pexGetStructureInfoReply *reply
			    = (pexGetStructureInfoReply *)(pPEXBuffer->pHead);

    LU_STRUCTURE(strmPtr->sid, pstr);

    err = InquireStructureInfo(	strmPtr->fpFormat, pstr, 
				(ddBitmask)(strmPtr->itemMask),
				&(reply->editMode), &(reply->elementPtr),
				&(reply->numElements), &(reply->lengthStructure),
				&(reply->hasRefs));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    reply->length = 0;
    WritePEXReplyToClient(  cntxtPtr, strmPtr,sizeof(pexGetStructureInfoReply),
			    reply);
    return( err );

} /* end-PEXGetStructureInfo() */

/*++	PEXSetStructurePermission
 --*/
ErrorCode
PEXSetStructurePermission( cntxtPtr, strmPtr )
pexContext     		*cntxtPtr;
pexSetStructurePermissionReq  *strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;

    LU_STRUCTURE(strmPtr->sid, pstr);

    err = SetStructurePermission( pstr, strmPtr->permission );
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    return( err );

} /* end-PEXSetStructurePermission() */

/*++	PEXGetElementInfo
 */
ErrorCode
PEXGetElementInfo (cntxtPtr, strmPtr )
pexContext		*cntxtPtr;
pexGetElementInfoReq	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;
    extern ddBuffer *pPEXBuffer;
    CARD32 numInfo;

    LU_STRUCTURE(strmPtr->sid, pstr);

    SETUP_INQ(pexGetElementInfoReply);

    err = InquireElementInfo(	pstr, (ddElementRange *)&(strmPtr->range),
				&numInfo, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetElementInfoReply);
	reply->numInfo = numInfo;
	WritePEXBufferReply(pexGetElementInfoReply);
    }
    return( err );

}/* end-PEXGetElementInfo */

/*++	PEXGetStructuresInNetwork
 --*/
ErrorCode
PEXGetStructuresInNetwork( cntxtPtr, strmPtr )
pexContext     			*cntxtPtr;
pexGetStructuresInNetworkReq    *strmPtr;
{
    ErrorCode err = PEXNOERR;
    diStructHandle pstr = 0;
    extern ddBuffer *pPEXBuffer;
    CARD32 numStructures;

    LU_STRUCTURE(strmPtr->sid, pstr);

    SETUP_INQ(pexGetStructuresInNetworkReply);

    err = InquireStructureNetwork(  pstr, strmPtr->which, &numStructures,
				    pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetStructuresInNetworkReply);
	reply->numStructures = numStructures;
	WritePEXBufferReply(pexGetStructuresInNetworkReply);
    }
    return( err );

} /* end-PEXGetStructuresInNetwork() */

/*++	PEXGetAncestors
 --*/
ErrorCode
PEXGetAncestors( cntxtPtr, strmPtr )
pexContext     		*cntxtPtr;
pexGetAncestorsReq    	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;
    extern ddBuffer *pPEXBuffer;
    CARD32 numPaths;

    LU_STRUCTURE(strmPtr->sid, pstr);

    SETUP_INQ(pexGetAncestorsReply);

    err = InquireAncestors( pstr, strmPtr->pathOrder, strmPtr->pathDepth,
			    &numPaths, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetAncestorsReply);
	reply->numPaths = numPaths;
	WritePEXBufferReply(pexGetAncestorsReply);
    }
    return( err );

} /* end-PEXGetAncestors() */

/*++	PEXGetDescendants
 --*/
ErrorCode
PEXGetDescendants( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexGetDescendantsReq    *strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;
    extern ddBuffer *pPEXBuffer;
    CARD32 numPaths;

    LU_STRUCTURE(strmPtr->sid, pstr);

    SETUP_INQ(pexGetDescendantsReply);

    err = InquireDescendants(	pstr, strmPtr->pathOrder, strmPtr->pathDepth,
				&numPaths, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetDescendantsReply);
	reply->numPaths = numPaths;
	WritePEXBufferReply(pexGetDescendantsReply);
    }
    return( err );

} /* end-PEXGetDescendants() */

/*++	PEXFetchElements
 --*/
ErrorCode
PEXFetchElements( cntxtPtr, strmPtr )
pexContext		*cntxtPtr;
pexFetchElementsReq	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;
    extern ddBuffer *pPEXBuffer;
    CARD32 numElements;

    LU_STRUCTURE(strmPtr->sid, pstr);

    SETUP_INQ(pexFetchElementsReply);

    err = InquireElements (pstr, &(strmPtr->range), &numElements, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexFetchElementsReply);
	reply->numElements = numElements;
	WritePEXBufferReply(pexFetchElementsReply);
    }
    return( err );

} /* end-PEXFetchElements */

/*++	PEXSetEditingMode
 --*/
ErrorCode
PEXSetEditingMode( cntxtPtr, strmPtr )
pexContext		*cntxtPtr;
pexSetEditingModeReq	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;

    LU_STRUCTURE(strmPtr->sid, pstr);

    err = SetEditMode (pstr, strmPtr->mode);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXSetEditingMode */

/*++	PEXSetElementPointer
 --*/
ErrorCode
PEXSetElementPointer( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexSetElementPointerReq	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;

    LU_STRUCTURE(strmPtr->sid, pstr);

    err = SetElementPointer (pstr, &(strmPtr->position));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXSetElementPointer() */

/*++	PEXSetElementPointerAtLabel
 --*/
ErrorCode
PEXSetElementPointerAtLabel( cntxtPtr, strmPtr )
pexContext      		*cntxtPtr;
pexSetElementPointerAtLabelReq  *strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;

    LU_STRUCTURE(strmPtr->sid, pstr);

    err = SetElementPointerAtLabel (pstr, strmPtr->label, strmPtr->offset);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXSetElementPointerAtLabel() */

/*++	PEXSetElementPointerAtPickID
 --*/
ErrorCode
PEXSetElementPointerAtPickID( cntxtPtr, strmPtr )
pexContext      		*cntxtPtr;
pexSetElementPointerAtPickIDReq  *strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;

    LU_STRUCTURE(strmPtr->sid, pstr);

    err = SetElementPointerAtPickID (pstr, strmPtr->pickId, strmPtr->offset);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXSetElementPointerAtLabel() */

/*++	PEXElementSearch
 --*/
ErrorCode
PEXElementSearch( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexElementSearchReq    	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;
    extern ddBuffer *pPEXBuffer;
    pexElementSearchReply *reply =
				(pexElementSearchReply *)(pPEXBuffer->pHead);
    CARD16 *pincl = (CARD16 *)(strmPtr + 1);

    LU_STRUCTURE(strmPtr->sid, pstr);

    SETUP_INQ(pexElementSearchReply);

    err = ElementSearch(    pstr, &(strmPtr->position), strmPtr->direction,
			    strmPtr->numIncls, strmPtr->numExcls, pincl,
			    pincl + strmPtr->numIncls + (strmPtr->numIncls %2),
			    &(reply->status),  &(reply->foundOffset));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
 
    reply->length = 0;
    WritePEXReplyToClient(cntxtPtr,strmPtr,sizeof(pexElementSearchReply),reply);
    return( err );

} /* end-PEXElementSearch() */

/*++	PEXStoreElements
 --*/
ErrorCode
PEXStoreElements( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexStoreElementsReq    	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;
    int i;
    pexElementInfo *pe = 0;
    CARD32 *curOC;
    pexStructure *ps;
    diStructHandle ph;
    pexOutputCommandError *pErr;

    LU_STRUCTURE(strmPtr->sid, pstr);
    CHECK_FP_FORMAT (strmPtr->fpFormat);

    for (i = 0, curOC = (CARD32 *)(strmPtr + 1);
	 i < strmPtr->numCommands;
	 i++, curOC += pe->length  ) {
	pe = (pexElementInfo *)curOC;
    	if ((PEXOCAll < pe->elementType ) && (pe->elementType <= PEXMaxOC)) { 
	    if (pe->elementType == PEXOCExecuteStructure) {
		ps = &(((pexExecuteStructure *)(pe))->id);
		LU_STRUCTURE(*ps, ph);
		*ps = (pexStructure)(ph);
	    }
	}
    }

    err = StoreElements(    pstr,
			    strmPtr->numCommands,(ddElementInfo *)(strmPtr+1),
			    &pErr);

    if (err == BadImplementation) PEX_ERR_EXIT(err,0,cntxtPtr)
    else if (err) PEX_OC_ERROR(pErr, cntxtPtr);

    return( err );

} /* end-PEXStoreElements() */

/*++	PEXDeleteElements
 --*/
ErrorCode
PEXDeleteElements( cntxtPtr, strmPtr )
pexContext		*cntxtPtr;
pexDeleteElementsReq	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;

    LU_STRUCTURE(strmPtr->sid, pstr);

    err = DeleteElements( pstr, &(strmPtr->range));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXDeleteElements */

/*++	PEXDeleteElementsToLabel
 --*/
ErrorCode
PEXDeleteElementsToLabel( cntxtPtr, strmPtr )
pexContext      		*cntxtPtr;
pexDeleteElementsToLabelReq    	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;

    LU_STRUCTURE(strmPtr->sid, pstr);

    err = DeleteToLabel (pstr, &(strmPtr->position), strmPtr->label);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXDeleteElementsToLabel() */

/*++	PEXDeleteBetweenLabels
 --*/
ErrorCode
PEXDeleteBetweenLabels( cntxtPtr, strmPtr )
pexContext     			*cntxtPtr;
pexDeleteBetweenLabelsReq    	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pstr = 0;

    LU_STRUCTURE(strmPtr->sid, pstr);

    err = DeleteBetweenLabels (pstr, strmPtr->label1, strmPtr->label2);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXDeleteBetweenLabels() */

/*++	PEXCopyElements
 --*/

ErrorCode
PEXCopyElements( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexCopyElementsReq  	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle ps1, ps2;

    LU_STRUCTURE (strmPtr->src, ps1);
    LU_STRUCTURE (strmPtr->dst, ps2);

    err = CopyElements (ps1, &(strmPtr->srcRange), ps2, &(strmPtr->dstPosition));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXCopyElements() */

/*++	Change Structure References
 --*/
ErrorCode
PEXChangeStructureRefs( cntxtPtr, strmPtr )
pexContext			*cntxtPtr;
pexChangeStructureRefsReq	*strmPtr;
{
    ErrorCode err = Success;
    diStructHandle pold = 0, pnew = 0;

    LU_STRUCTURE(strmPtr->old_id, pold);
    LU_STRUCTURE(strmPtr->new_id, pnew);

    err = ChangeStructureReferences (pold, pnew);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXChangeStructureRefs() */
/*++
 *
 *	End of File
 *
 --*/
