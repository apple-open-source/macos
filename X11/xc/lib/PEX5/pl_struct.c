/* $Xorg: pl_struct.c,v 1.5 2001/02/09 02:03:29 xorgcvs Exp $ */

/******************************************************************************

Copyright 1992, 1998  The Open Group

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


Copyright 1987,1991 by Digital Equipment Corporation, Maynard, Massachusetts

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and that
the name of Digital not be used in advertising or publicity
pertaining to distribution of the software without specific, written prior
permission.  Digital make no representations about the suitability
of this software for any purpose.  It is provided "as is" without express or
implied warranty.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************************/

#include "PEXlib.h"
#include "PEXlibint.h"
#include "pl_oc_util.h"


PEXStructure
PEXCreateStructure (display)

INPUT Display		*display;

{
    register pexCreateStructureReq	*req;
    char				*pBuf;
    PEXStructure			sid;


    /*
     * Get a structure resource id from X.
     */

    sid = XAllocID (display);


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CreateStructure, pBuf);

    BEGIN_REQUEST_HEADER (CreateStructure, pBuf, req);

    PEXStoreReqHead (CreateStructure, req);
    req->id = sid;

    END_REQUEST_HEADER (CreateStructure, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (sid);
}


void
PEXDestroyStructures (display, numStructures, structures)

INPUT Display		*display;
INPUT unsigned long	numStructures;
INPUT PEXStructure	*structures;

{
    register pexDestroyStructuresReq	*req;
    char				*pBuf;
    int 				size;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    size = numStructures * SIZEOF (pexStructure);
    PEXGetReqExtra (DestroyStructures, size, pBuf);

    BEGIN_REQUEST_HEADER (DestroyStructures, pBuf, req);

    PEXStoreReqExtraHead (DestroyStructures, size, req);
    req->numStructures = numStructures;

    END_REQUEST_HEADER (DestroyStructures, pBuf, req);

    STORE_LISTOF_CARD32 (numStructures, structures, pBuf);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXCopyStructure (display, srcStructure, destStructure)

INPUT Display		*display;
INPUT PEXStructure	srcStructure;
INPUT PEXStructure	destStructure;

{
    register pexCopyStructureReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CopyStructure, pBuf);

    BEGIN_REQUEST_HEADER (CopyStructure, pBuf, req);

    PEXStoreReqHead (CopyStructure, req);
    req->src = srcStructure;
    req->dst = destStructure;

    END_REQUEST_HEADER (CopyStructure, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


Status
PEXGetStructureInfo (display, structure, float_format,
    value_mask, info_return)

INPUT Display			*display;
INPUT PEXStructure		structure;
INPUT int			float_format;
INPUT unsigned long		value_mask;
OUTPUT PEXStructureInfo		*info_return;

{
    register pexGetStructureInfoReq	*req;
    char				*pBuf;
    pexGetStructureInfoReply		rep;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetStructureInfo, pBuf);

    BEGIN_REQUEST_HEADER (GetStructureInfo, pBuf, req);

    PEXStoreReqHead (GetStructureInfo, req);
    req->fpFormat = float_format;
    req->sid = structure;
    req->itemMask = value_mask;

    END_REQUEST_HEADER (GetStructureInfo, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xTrue) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	return (0);            /* return an error */
    }


    if (value_mask & PEXEditMode)
	info_return->edit_mode = rep.editMode;
    if (value_mask & PEXElementPtr)
	info_return->element_pointer = rep.elementPtr;
    if (value_mask & PEXNumElements)
	info_return->element_count = rep.numElements;
    if (value_mask & PEXLengthStructure)
	info_return->size = rep.lengthStructure;
    if (value_mask & PEXHasRefs)
	info_return->has_refs = rep.hasRefs;


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


Status
PEXGetElementInfo (display, structure, whence1, offset1, whence2, offset2,
    float_format, numElementInfoReturn, infoReturn)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT int		whence1;
INPUT long		offset1;
INPUT int		whence2;
INPUT long		offset2;
INPUT int		float_format;
OUTPUT unsigned long	*numElementInfoReturn;
OUTPUT PEXElementInfo	**infoReturn;

{
    register pexGetElementInfoReq	*req;
    char				*pBuf;
    pexGetElementInfoReply		rep;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetElementInfo, pBuf);

    BEGIN_REQUEST_HEADER (GetElementInfo, pBuf, req);

    PEXStoreReqHead (GetElementInfo, req);
    req->fpFormat = float_format;
    req->sid = structure;
    req->position1_whence = whence1;
    req->position1_offset = offset1;
    req->position2_whence = whence2;
    req->position2_offset = offset2;

    END_REQUEST_HEADER (GetElementInfo, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*numElementInfoReturn = 0;
	*infoReturn = NULL;
	return (0);        /* return an error */
    }

    *numElementInfoReturn = rep.numInfo;


    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    *infoReturn = (PEXElementInfo *) Xmalloc (
        (unsigned) (sizeof (PEXElementInfo) * rep.numInfo));

    XREAD_LISTOF_ELEMINFO (display, rep.numInfo, (*infoReturn));


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


PEXStructure *
PEXGetStructuresInNetwork (display, structure, which, numStructuresReturn)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT int		which;
OUTPUT unsigned long	*numStructuresReturn;

{
    register pexGetStructuresInNetworkReq	*req;
    char					*pBuf;
    pexGetStructuresInNetworkReply		rep;
    PEXStructure				*structsRet;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetStructuresInNetwork, pBuf);

    BEGIN_REQUEST_HEADER (GetStructuresInNetwork, pBuf, req);

    PEXStoreReqHead (GetStructuresInNetwork, req);
    req->sid = structure;
    req->which = which;

    END_REQUEST_HEADER (GetStructuresInNetwork, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*numStructuresReturn = 0;
	return (NULL);             /* return an error */
    }

    *numStructuresReturn = rep.numStructures;


    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    structsRet = (PEXStructure *) Xmalloc (
        (unsigned) (sizeof (PEXStructure) * rep.numStructures));

    XREAD_LISTOF_CARD32 (display, rep.numStructures, structsRet);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (structsRet);
}


PEXStructurePath *
PEXGetAncestors (display, structure, pathPart, pathDepth, numPathsReturn)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT int		pathPart;
INPUT unsigned long	pathDepth;
OUTPUT unsigned long	*numPathsReturn;

{
    register pexGetAncestorsReq		*req;
    char				*pBuf, *pBufSave;
    pexGetAncestorsReply		rep;
    PEXStructurePath			*pStrucPath;
    PEXElementRef			*pElemRef;
    int					numElements;
    int					i;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetAncestors, pBuf);

    BEGIN_REQUEST_HEADER (GetAncestors, pBuf, req);

    PEXStoreReqHead (GetAncestors, req);
    req->sid = structure;
    req->pathOrder = pathPart;
    req->pathDepth = pathDepth;

    END_REQUEST_HEADER (GetAncestors, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*numPathsReturn = 0;
	return (NULL);           /* return an error */
    }

    *numPathsReturn = rep.numPaths;


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    pStrucPath = (PEXStructurePath *) Xmalloc (
	(unsigned) (rep.numPaths * sizeof (PEXStructurePath)));

    for (i = 0; i < rep.numPaths; i++)
    {
	EXTRACT_CARD32 (pBuf, numElements);

	pElemRef = (PEXElementRef *) Xmalloc (
	    (unsigned) (numElements * sizeof (PEXElementRef)));

	EXTRACT_LISTOF_ELEMREF (numElements, pBuf, pElemRef);

	pStrucPath[i].count = numElements;
	pStrucPath[i].elements = pElemRef;
    }

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (pStrucPath);
}



PEXStructurePath *
PEXGetDescendants (display, structure, pathPart, pathDepth, numPathsReturn)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT int		pathPart;
INPUT unsigned long	pathDepth;
OUTPUT unsigned long	*numPathsReturn;

{
    register pexGetDescendantsReq	*req;
    char				*pBuf, *pBufSave;
    pexGetDescendantsReply		rep;
    PEXStructurePath			*pStrucPath;
    PEXElementRef			*pElemRef;
    int					numElements;
    int					i;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetDescendants, pBuf);

    BEGIN_REQUEST_HEADER (GetDescendants, pBuf, req);

    PEXStoreReqHead (GetDescendants, req);
    req->sid = structure;
    req->pathOrder = pathPart;
    req->pathDepth = pathDepth;

    END_REQUEST_HEADER (GetDescendants, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*numPathsReturn = 0;
	return (NULL);          /* return an error */
    }

    *numPathsReturn = rep.numPaths;


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer to pass the replies back to the client.
     */

    pStrucPath = (PEXStructurePath *) Xmalloc (
	(unsigned) (rep.numPaths * sizeof (PEXStructurePath)));

    for (i = 0; i < rep.numPaths; i++)
    {
	EXTRACT_CARD32 (pBuf, numElements);

	pElemRef = (PEXElementRef *) Xmalloc (
	    (unsigned) (numElements * sizeof (PEXElementRef)));

	EXTRACT_LISTOF_ELEMREF (numElements, pBuf, pElemRef);

	pStrucPath[i].count = numElements;
	pStrucPath[i].elements = pElemRef;
    }

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (pStrucPath);
}


Status
PEXFetchElements (display, structure, whence1, offset1, whence2, offset2,
    float_format, numElementsReturn, sizeReturn, ocsReturn)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT int		whence1;
INPUT long		offset1;
INPUT int		whence2;
INPUT long		offset2;
INPUT int		float_format;
OUTPUT unsigned long	*numElementsReturn;
OUTPUT unsigned long	*sizeReturn;
OUTPUT char		**ocsReturn;

{
    register pexFetchElementsReq	*req;
    char				*pBuf;
    pexFetchElementsReply		rep;
    long				repSize;
    PEXOCData				*oc_data;
    int					server_float_format;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    server_float_format = PEXGetProtocolFloatFormat (display);

    PEXGetReq (FetchElements, pBuf);

    BEGIN_REQUEST_HEADER (FetchElements, pBuf, req);

    PEXStoreReqHead (FetchElements, req);
    req->fpFormat = server_float_format;
    req->sid = structure;
    req->position1_whence = whence1;
    req->position1_offset = offset1;
    req->position2_whence = whence2;
    req->position2_offset = offset2;

    END_REQUEST_HEADER (FetchElements, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
    	*sizeReturn = *numElementsReturn = 0;
	*ocsReturn = NULL;
        return (0);		/* return an error */
    }

    *numElementsReturn = rep.numElements;

    if (server_float_format != float_format)
    {
	/*
	 * Convert from server's float format to the float format
	 * specified by the application.
	 */

	XREAD_INTO_SCRATCH (display, pBuf, rep.length << 2);

	oc_data = PEXDecodeOCs (server_float_format, rep.numElements,
            rep.length << 2, pBuf);

	FINISH_WITH_SCRATCH (display, pBuf, rep.length << 2);

	*ocsReturn = PEXEncodeOCs (float_format, rep.numElements,
            oc_data, sizeReturn);

	PEXFreeOCData (rep.numElements, oc_data);
    }
    else
    {
	/*
	 * No float conversion necessary.
	 */

	*sizeReturn = repSize = rep.length << 2;
	if ((*ocsReturn = (char *) Xmalloc ((unsigned) repSize)))
	    _XRead (display, *ocsReturn, (long) repSize);
    }


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


Status
PEXFetchElementsAndSend (display, structure,
    whence1, offset1, whence2, offset2, dstDisplay, resID, reqType)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT int		whence1;
INPUT long		offset1;
INPUT int		whence2;
INPUT long		offset2;
INPUT Display		*dstDisplay;
INPUT XID		resID;
INPUT PEXOCRequestType	reqType;

{
    register pexFetchElementsReq	*req;
    char				*pBuf;
    pexFetchElementsReply		rep;
    PEXDisplayInfo 			*srcDisplayInfo;
    PEXDisplayInfo			*dstDisplayInfo;
    char	 			*ocAddr;
    PEXOCData				*oc_data;
    PEXEnumTypeDesc			*srcFloats;
    PEXEnumTypeDesc			*dstFloats;
    long				bytesLeft;
    int					getSize;
    int					size;
    unsigned long			oc_size;
    int					i, j;
    int					fp_match;
    int					float_format;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Determine which floating point format to use.
     */

    PEXGetDisplayInfo (display, srcDisplayInfo);
    PEXGetDisplayInfo (dstDisplay, dstDisplayInfo);

    if (srcDisplayInfo->fpFormat == dstDisplayInfo->fpFormat)
    {
	float_format = dstDisplayInfo->fpFormat;
	fp_match = 1;
    }
    else
    {
	srcFloats = srcDisplayInfo->fpSupport;
	dstFloats = dstDisplayInfo->fpSupport;

	fp_match = 0;
	for (i = 0; i < dstDisplayInfo->fpCount && !fp_match; i++)
	    for (j = 0; j < srcDisplayInfo->fpCount; j++)
	    {
		if (dstFloats[i].index == srcFloats[j].index)
		{
		    float_format = dstFloats[i].index;
		    fp_match = 1;
		    break;
		}
	    }

	if (!fp_match)
	{
	    /*
	     * Will have to convert from source display float format to
	     * destination display float format.
	     */

	    float_format = srcDisplayInfo->fpFormat;
	}
    }


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (FetchElements, pBuf);

    BEGIN_REQUEST_HEADER (FetchElements, pBuf, req);

    PEXStoreReqHead (FetchElements, req);
    req->fpFormat = float_format;
    req->sid = structure;
    req->position1_whence = whence1;
    req->position1_offset = offset1;
    req->position2_whence = whence2;
    req->position2_offset = offset2;

    END_REQUEST_HEADER (FetchElements, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
        return (0);		/* return an error */
    }


    /*
     * If no floating point conversion has to take place, fetch the element
     * info directly into the destination display connection.
     */

    if (fp_match)
    {
	if (display == dstDisplay)
	    UnlockDisplay (display);

	if (PEXStartOCs (dstDisplay, resID, reqType, float_format,
	    rep.numElements, rep.length))
	{
	    bytesLeft = rep.length << 2;
	    getSize = PEXGetOCAddrMaxSize (dstDisplay);

	    while (bytesLeft > 0)
	    {
		size = bytesLeft < getSize ? bytesLeft : getSize;
		ocAddr = PEXGetOCAddr (dstDisplay, size);
		_XRead (display, ocAddr, (long) size);
		bytesLeft -= size;
	    }

	    PEXFinishOCs (dstDisplay);
	}

	if (display != dstDisplay)
	    UnlockDisplay (display);
    }
    else
    {
	/*
	 * Floating point conversion necessary.
	 */

	XREAD_INTO_SCRATCH (display, pBuf, rep.length << 2);

	oc_data = PEXDecodeOCs (float_format, rep.numElements,
            rep.length << 2, pBuf);

	FINISH_WITH_SCRATCH (display, pBuf, rep.length << 2);

	pBuf = PEXEncodeOCs (dstDisplayInfo->fpFormat, rep.numElements,
            oc_data, &oc_size);

	PEXFreeOCData (rep.numElements, oc_data);

	if (display == dstDisplay)
	    UnlockDisplay (display);

	PEXSendOCs (dstDisplay, resID, reqType, dstDisplayInfo->fpFormat,
	    rep.numElements, oc_size, pBuf);

	if (display != dstDisplay)
	    UnlockDisplay (display);
    }

    PEXSyncHandle (dstDisplay);

    return (1);
}


void
PEXSetEditingMode (display, structure, mode)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT int		mode;

{
    register pexSetEditingModeReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (SetEditingMode, pBuf);

    BEGIN_REQUEST_HEADER (SetEditingMode, pBuf, req);

    PEXStoreReqHead (SetEditingMode, req);
    req->sid = structure;
    req->mode = mode;

    END_REQUEST_HEADER (SetEditingMode, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXSetElementPtr (display, structure, whence, offset)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT int		whence;
INPUT long		offset;

{
    register pexSetElementPointerReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (SetElementPointer, pBuf);

    BEGIN_REQUEST_HEADER (SetElementPointer, pBuf, req);

    PEXStoreReqHead (SetElementPointer, req);
    req->sid = structure;
    req->position_whence = whence;
    req->position_offset = offset;

    END_REQUEST_HEADER (SetElementPointer, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXSetElementPtrAtLabel (display, structure, label, offset)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT long		label;
INPUT long		offset;

{
    register pexSetElementPointerAtLabelReq	*req;
    char					*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (SetElementPointerAtLabel, pBuf);

    BEGIN_REQUEST_HEADER (SetElementPointerAtLabel, pBuf, req);

    PEXStoreReqHead (SetElementPointerAtLabel, req);
    req->sid = structure;
    req->label = label;
    req->offset = offset;

    END_REQUEST_HEADER (SetElementPointerAtLabel, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


Status
PEXElementSearch (display, structure, whence, offset, direction,
	numIncl, inclList, numExcl, exclList, offsetReturn)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT int		whence;
INPUT long		offset;
INPUT int		direction;
INPUT unsigned long	numIncl;
INPUT unsigned short	*inclList;
INPUT unsigned long	numExcl;
INPUT unsigned short	*exclList;
OUTPUT unsigned long	*offsetReturn;

{
    register pexElementSearchReq	*req;
    char				*pBuf;
    pexElementSearchReply		rep;
    int 				size;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    size = SIZEOF (CARD16) *
	(numIncl + (numIncl & 1) + numExcl + (numExcl & 1));

    PEXGetReqExtra (ElementSearch, size, pBuf);

    BEGIN_REQUEST_HEADER (ElementSearch, pBuf, req);

    PEXStoreReqExtraHead (ElementSearch, size, req);
    req->sid = structure;
    req->position_whence = whence;
    req->position_offset = offset;
    req->direction = direction;
    req->numIncls = numIncl;
    req->numExcls = numExcl;

    END_REQUEST_HEADER (ElementSearch, pBuf, req);

    STORE_LISTOF_CARD16 (numIncl, inclList, pBuf);
    pBuf += ((numIncl & 1) * SIZEOF (CARD16));
    STORE_LISTOF_CARD16 (numExcl, exclList, pBuf);

    if (_XReply (display, (xReply *)&rep, 0, xTrue) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*offsetReturn = 0;
	return (0);               /* return an error */
    }

    *offsetReturn = rep.foundOffset;


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (rep.status);
}


void
PEXDeleteElements (display, structure, whence1, offset1, whence2, offset2)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT int		whence1;
INPUT long		offset1;
INPUT int		whence2;
INPUT long		offset2;

{
    register pexDeleteElementsReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (DeleteElements, pBuf);

    BEGIN_REQUEST_HEADER (DeleteElements, pBuf, req);

    PEXStoreReqHead (DeleteElements, req);
    req->sid =  structure;
    req->position1_whence = whence1;
    req->position1_offset = offset1;
    req->position2_whence = whence2;
    req->position2_offset = offset2;

    END_REQUEST_HEADER (DeleteElements, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXDeleteToLabel (display, structure, whence, offset, label)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT int		whence;
INPUT long		offset;
INPUT long		label;

{
    register pexDeleteElementsToLabelReq	*req;
    char					*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (DeleteElementsToLabel, pBuf);

    BEGIN_REQUEST_HEADER (DeleteElementsToLabel, pBuf, req);

    PEXStoreReqHead (DeleteElementsToLabel, req);
    req->sid =  structure;
    req->position_whence = whence;
    req->position_offset = offset;
    req->label = label;

    END_REQUEST_HEADER (DeleteElementsToLabel, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXDeleteBetweenLabels (display, structure, label1, label2)

INPUT Display		*display;
INPUT PEXStructure	structure;
INPUT long		label1;
INPUT long		label2;

{
    register pexDeleteBetweenLabelsReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (DeleteBetweenLabels, pBuf);

    BEGIN_REQUEST_HEADER (DeleteBetweenLabels, pBuf, req);

    PEXStoreReqHead (DeleteBetweenLabels, req);
    req->sid =  structure;
    req->label1 = label1;
    req->label2 = label2;

    END_REQUEST_HEADER (DeleteBetweenLabels, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXCopyElements (display, srcStructure, srcWhence1, srcOffset1, srcWhence2,
    srcOffset2, destStructure, destWhence, destOffset)

INPUT Display		*display;
INPUT PEXStructure	srcStructure;
INPUT int		srcWhence1;
INPUT long		srcOffset1;
INPUT int		srcWhence2;
INPUT long		srcOffset2;
INPUT PEXStructure	destStructure;
INPUT int		destWhence;
INPUT long		destOffset;

{
    register pexCopyElementsReq		*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CopyElements, pBuf);

    BEGIN_REQUEST_HEADER (CopyElements, pBuf, req);

    PEXStoreReqHead (CopyElements, req);
    req->src = srcStructure;
    req->srcPosition1_whence = srcWhence1;
    req->srcPosition1_offset = srcOffset1;
    req->srcPosition2_whence = srcWhence2;
    req->srcPosition2_offset = srcOffset2;
    req->dst = destStructure;
    req->dstPosition_whence = destWhence;
    req->dstPosition_offset = destOffset;

    END_REQUEST_HEADER (CopyElements, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXChangeStructureRefs (display, oldStructure, newStructure)

INPUT Display		*display;
INPUT PEXStructure	oldStructure;
INPUT PEXStructure	newStructure;

{
    register pexChangeStructureRefsReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (ChangeStructureRefs, pBuf);

    BEGIN_REQUEST_HEADER (ChangeStructureRefs, pBuf, req);

    PEXStoreReqHead (ChangeStructureRefs, req);
    req->old_id = oldStructure;
    req->new_id = newStructure;

    END_REQUEST_HEADER (ChangeStructureRefs, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}
