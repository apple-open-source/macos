/* $Xorg: pexLut.c,v 1.4 2001/02/09 02:04:14 xorgcvs Exp $ */
/*

Copyright 1989, 1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.


Copyright 1989, 1990, 1991 by Sun Microsystems, Inc.
 
                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/
/* $XFree86: xc/programs/Xserver/PEX5/dipex/objects/pexLut.c,v 3.9 2001/12/14 19:57:43 dawes Exp $ */


/*++
 *	PEXCreateLookupTable
 *	PEXCopyLookupTable
 *	PEXFreeLookupTable
 *	PEXGetTableInfo
 *	PEXGetPredefinedEntries
 *	PEXGetDefinedIndices
 *	PEXGetTableEntry
 *	PEXGetTableEntries
 *	PEXSetTableEntries
 *	PEXDeleteTableEntries
 --*/

#include "X.h"
#include "Xproto.h"
#include "pexError.h"
#include "PEXproto.h"
#include "dipex.h"
#include "pex_site.h"
#include "pexLookup.h"
#include "pexos.h"


#define	VALID_TABLETYPE(type)	((type > 0) && (type <= PEXMaxTableType))


#ifdef min
#undef min
#endif
 
#ifdef max
#undef max
#endif



/*++	PEXCreateLookupTable
 --*/
ErrorCode
PEXCreateLookupTable (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;	/* context pointer 	*/
pexCreateLookupTableReq	*strmPtr;	/* stream pointer 	*/
{
    ErrorCode freeLUT ();
    ErrorCode err = Success; 
    DrawablePtr pdraw = 0;
    diLUTHandle lutptr = 0;

    if (!VALID_TABLETYPE(strmPtr->tableType))
	PEX_ERR_EXIT(BadValue,strmPtr->tableType,cntxtPtr);

    if (!LegalNewID(strmPtr->lut, cntxtPtr->client))
	PEX_ERR_EXIT(BadIDChoice,strmPtr->lut,cntxtPtr);

    LU_DRAWABLE(strmPtr->drawableExample, pdraw);

    lutptr = (diLUTHandle) xalloc ((unsigned long)sizeof(ddLUTResource));
    if (!lutptr) PEX_ERR_EXIT (BadAlloc,0,cntxtPtr);
    lutptr->id = strmPtr->lut;
    lutptr->lutType = strmPtr->tableType;
    
    err = CreateLUT( pdraw, lutptr);
    if (err) {
	xfree((pointer)lutptr);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    }

    ADDRESOURCE (strmPtr->lut, PEXLutType, lutptr);

    return( err );

} /* end-PEXCreateLookupTable() */

/*++	PEXCopyLookupTable
 --*/
ErrorCode
PEXCopyLookupTable (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;	/* Context Pointer      */
pexCopyLookupTableReq	*strmPtr;	/* Stream Pointer 	*/
{
    diLUTHandle lsrc, ldest;
    ErrorCode err = Success;

    LU_TABLE(strmPtr->src, lsrc);
    LU_TABLE(strmPtr->dst, ldest);

    err = CopyLUT (lsrc, ldest);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    return (err);

} /* end-PEXCopyLookupTable() */

/*++	PEXFreeLookupTable
 --*/
ErrorCode
PEXFreeLookupTable (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;	/* Context Pointer 	*/
pexFreeLookupTableReq   *strmPtr;	/* Stream Pointer 	*/
{
    ErrorCode err = Success;
    diLUTHandle l = 0;

    if ((strmPtr == NULL) || (strmPtr->id == 0)) {
	err = PEX_ERROR_CODE(PEXLookupTableError);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    }

    LU_TABLE(strmPtr->id, l);

    FreeResource(strmPtr->id, RT_NONE);

    return( err );

} /* end-PEXFreeLookupTable() */

/*++	PEXGetTableInfoReq
 --*/

ErrorCode
PEXGetTableInfo( cntxtPtr, strmPtr )
pexContext 	 	*cntxtPtr;
pexGetTableInfoReq	*strmPtr;
{
    ErrorCode err = Success;
    DrawablePtr pdraw;
    extern ddBufferPtr pPEXBuffer;
    pexGetTableInfoReply *reply = (pexGetTableInfoReply *)(pPEXBuffer->pHead);

    if (!VALID_TABLETYPE(strmPtr->tableType))
	PEX_ERR_EXIT(BadValue,strmPtr->tableType,cntxtPtr);

    LU_DRAWABLE(strmPtr->drawableExample, pdraw);

    SETUP_INQ(pexGetTableInfoReply);

    err = InquireLUTInfo(   pdraw, strmPtr->tableType,
			    (pexTableInfo *)&(reply->definableEntries));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    reply->length = 0;
    WritePEXReplyToClient(  cntxtPtr, strmPtr,
			    sizeof(pexGetTableInfoReply) + reply->length,
			    reply);
    return( err );

} /* end-PEXGetTableInfo() */

/*++	PEXGetPredefinedEntries
 --*/

ErrorCode
PEXGetPredefinedEntries( cntxtPtr, strmPtr )
pexContext 			*cntxtPtr;
pexGetPredefinedEntriesReq 	*strmPtr;
{
    ErrorCode err = Success;
    extern ddBufferPtr pPEXBuffer;
    DrawablePtr pdraw = 0;
    ddULONG numEntries =  (ddULONG)(strmPtr->count);

    if (!VALID_TABLETYPE(strmPtr->tableType))
	PEX_ERR_EXIT(BadValue,strmPtr->tableType,cntxtPtr);

    LU_DRAWABLE(strmPtr->drawableExample, pdraw);

    SETUP_INQ(pexGetPredefinedEntriesReply);

    err = InquireLUTPredEntries(    pdraw, strmPtr->tableType,
				    strmPtr->start, strmPtr->count,
				    &numEntries, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetPredefinedEntriesReply);
	reply->numEntries = numEntries;
	WritePEXBufferReply(pexGetPredefinedEntriesReply);
    }
    return( err );

} /* end-PEXGetPredefinedEntries() */

/*++	PEXGetDefinedIndices
 --*/

ErrorCode
PEXGetDefinedIndices( cntxtPtr, strmPtr)
pexContext   	 	*cntxtPtr;
pexGetDefinedIndicesReq	*strmPtr;
{
    ErrorCode err = Success;
    diLUTHandle pf;
    extern ddBufferPtr pPEXBuffer;
    ddULONG numIndices = 0;

    LU_TABLE(strmPtr->id, pf);

    SETUP_INQ(pexGetDefinedIndicesReply);

    err = InquireLUTIndices( pf, &numIndices, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetDefinedIndicesReply);
	reply->numIndices = numIndices;
	WritePEXBufferReply(pexGetDefinedIndicesReply);
    }
    return( err );

} /* end-PEXGetDefinedIndices() */

/*++	PEXGetTableEntry
 --*/

ErrorCode
PEXGetTableEntry( cntxtPtr, strmPtr )
pexContext 		*cntxtPtr;
pexGetTableEntryReq	*strmPtr;
{
    ErrorCode err = Success;
    diLUTHandle pf;
    extern ddBufferPtr pPEXBuffer;
    ddUSHORT status;

    LU_TABLE(strmPtr->lut, pf);

    SETUP_INQ(pexGetTableEntryReply);

    err = InquireLUTEntry(  pf, strmPtr->index, strmPtr->valueType,
			    &status, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    /*
     * If this is a font table, we have to convert font handles to font ids.
     */

    if (pf->lutType == PEXTextFontLUT) {
	int i;
	pexTextFontEntry *ptfe = (pexTextFontEntry *)(pPEXBuffer->pBuf);
	pexFont *ptr = (pexFont *)(ptfe + 1);
	for (i=0; i<ptfe->numFonts; i++, ptr++)
	    *ptr = ((diFontHandle) *ptr)->id;
    }

    {
	SETUP_VAR_REPLY(pexGetTableEntryReply);
	reply->status = status;
	reply->tableType = pf->lutType;
	WritePEXBufferReply(pexGetTableEntryReply);
    }
    return( err );

} /* end-PEXGetTableEntry() */

/*++	PEXGetTableEntries
 --*/
ErrorCode
PEXGetTableEntries( cntxtPtr, strmPtr )
pexContext 	 	*cntxtPtr;
pexGetTableEntriesReq 	*strmPtr;
{
    ErrorCode err = Success;
    diLUTHandle pf;
    ddULONG numEntries;
    extern ddBuffer *pPEXBuffer;

    LU_TABLE(strmPtr->lut, pf);

    SETUP_INQ(pexGetTableEntriesReply);

    err = InquireLUTEntries(	pf, strmPtr->start, strmPtr->count,
				strmPtr->valueType, &numEntries, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    /*
     * If this is a font table, we have to convert font handles to font ids.
     */

    if (pf->lutType == PEXTextFontLUT) {
	int i, j;
	pexTextFontEntry *ptfe = (pexTextFontEntry *)(pPEXBuffer->pBuf);
 	for (i=0; i<strmPtr->count; i++) {
 	    pexFont *ptr = (pexFont *)(ptfe + 1);
 	    for (j=0; j<ptfe->numFonts; j++, ptr++)
 		*ptr = ((diFontHandle) *ptr)->id;
 	    ptfe = (pexTextFontEntry *) ptr;
  	}
    }

    {
	SETUP_VAR_REPLY(pexGetTableEntriesReply);
	reply->tableType = pf->lutType;
	reply->numEntries = numEntries;
	WritePEXBufferReply(pexGetTableEntriesReply);
    }
    return( err );

} /* end-PEXGetTableEntries() */

/*++	PEXSetTableEntries
 --*/

ErrorCode
PEXSetTableEntries( cntxtPtr, strmPtr )
pexContext 		*cntxtPtr;
pexSetTableEntriesReq 	*strmPtr;
{
    ErrorCode err = Success;
    diLUTHandle pf = 0;

    LU_TABLE(strmPtr->lut, pf);
    CHECK_FP_FORMAT(strmPtr->fpFormat);

    /*
	If this is a font table, lookup font id's and stuff pointers into
	the that longword, so ddpex gets its handles instead of ids.
     */
    if (pf->lutType == PEXTextFontLUT) {
	int i, j;
	diFontHandle fh;
	pexTextFontEntry *ptfe = (pexTextFontEntry *)(strmPtr + 1);
 	for (i=0; i<strmPtr->count; i++) {
 	    pexFont *ptr = (pexFont *)(ptfe + 1);
 	    for (j=0; j<ptfe->numFonts; j++, ptr++) {
 		LU_PEXFONT(*ptr, fh);
 		*ptr = (pexFont) fh;
 	    }
 	    ptfe = (pexTextFontEntry *) ptr;
  	}
    }

    err = SetLUTEntries(    pf, strmPtr->start, strmPtr->count,
			    (ddPointer)(strmPtr + 1));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXSetTableEntries() */

/*++	PEXDeleteTableEntries
 --*/

ErrorCode
PEXDeleteTableEntries( cntxtPtr, strmPtr )
pexContext			*cntxtPtr;
pexDeleteTableEntriesReq 	*strmPtr;
{
    ErrorCode err = Success;
    diLUTHandle pf;

    LU_TABLE(strmPtr->lut, pf);
    err = DeleteLUTEntries (pf, strmPtr->start, strmPtr->count);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXDeleteTableEntries() */

/*++	PEXChangeTableValues
 --*/
ErrorCode
PEXChangeTableValues (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;	/* context pointer 	*/
pexChangeTableValuesReq	*strmPtr;	/* stream pointer 	*/
{
    ErrorCode err = Success;
    diLUTHandle pf = 0;

    LU_TABLE(strmPtr->lut, pf);
    CHECK_FP_FORMAT(strmPtr->fpFormat);

    /*
	If this is a font table, lookup font id's and stuff pointers into
	the that longword, so ddpex gets its handles instead of ids.
     */
    if  ( (pf->lutType == PEXTextFontLUT) && 
	  (strmPtr->TableMask == PEXLUTVTextFontGroup) )
    {
	int i, j;
	diFontHandle fh;
	pexTextFontEntry *ptfe = (pexTextFontEntry *)(strmPtr + 1);
 	    pexFont *ptr = (pexFont *)(ptfe + 1);
 	    for (j=0; j<ptfe->numFonts; j++, ptr++) {
 		LU_PEXFONT(*ptr, fh);
 		*ptr = (pexFont) fh;
 	    }
    }

    /* call to change table values routine goes here, this is a shell
	for this routine but should contain all necessary information
	to process the request

    err = ChangeTableValues ( pf, strmPtr->length, strmPtr->index,
			      strmPtr->TableMask, (ddPointer)(strmPtr + 1));
    */
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXChangeTableValues() */

/*++	PEXGetTableValues
 --*/
ErrorCode
PEXGetTableValues( cntxtPtr, strmPtr )
pexContext 	 	*cntxtPtr;
pexGetTableValuesReq 	*strmPtr;
{
    ErrorCode err = Success;
    diLUTHandle pf;
    ddULONG numValues;
    ddUSHORT status;
    extern ddBuffer *pPEXBuffer;

    CHECK_FP_FORMAT(strmPtr->fpFormat);
    LU_TABLE(strmPtr->lut, pf);

    SETUP_INQ(pexGetTableValuesReply);

    /* Place holder for ddpex interface
    err =  GetLUTValues( pf, strmPtr->index, strmPtr->TableMask,
			 strmPtr->valueType, &numValues, &status, pPEXBuffer);
    */

    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    /*
     * If this is a font table, we have to convert font handles to font ids.
     */

    if  ( (pf->lutType == PEXTextFontLUT) && 
	  (strmPtr->TableMask == PEXLUTVTextFontGroup) )
    {
	int i, j;
	pexTextFontEntry *ptfe = (pexTextFontEntry *)(pPEXBuffer->pBuf);
 	    pexFont *ptr = (pexFont *)(ptfe + 1);
 	    for (j=0; j<ptfe->numFonts; j++, ptr++)
 		*ptr = ((diFontHandle) *ptr)->id;
    }

    {
	SETUP_VAR_REPLY(pexGetTableValuesReply);
	reply->tableType = pf->lutType;
	reply->numValues = numValues;
	reply->status = status;
	WritePEXBufferReply(pexGetTableValuesReply);
    }
    return( err );

} /* end-PEXGetTableValues() */
/*++
 *
 *	End of File
 *
 --*/
