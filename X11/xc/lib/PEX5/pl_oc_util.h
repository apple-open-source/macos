/* $Xorg: pl_oc_util.h,v 1.4 2001/02/09 02:03:28 xorgcvs Exp $ */

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


/*
 * NAME:
 *	PEXCopyBytesToOC
 *
 * ARGUMENTS:
 *	_display	The display pointer.
 *
 *	_numBytes	The number of bytes to copy.
 *
 *	_data		The data to copy.
 *
 * DESCRIPTION:
 *	This macro serves the same purpose as the PEXlib function
 *	PEXCopyBytesToOC, but is used internally by PEXlib in order to
 *	avoid a function call (in the simple case when there is enough
 *	space in the X transport buffer to do the copy).
 */

#ifndef DEBUG      /* use the function version when debugging */

#define PEXCopyBytesToOC(_display, _numBytes, _data) \
{ \
    if ((int) (_numBytes) <= (int) BytesLeftInXBuffer (_display)) \
    { \
	memcpy (_display->bufptr, _data, _numBytes); \
	_display->bufptr += _numBytes; \
    } \
    else \
    { \
	_PEXSendBytesToOC (_display, _numBytes, _data); \
    } \
}

#endif



/*
 * NAME:
 *	PEXCopyWordsToOC
 *
 * ARGUMENTS:
 *	_display	The display pointer.
 *
 *	_numWords	The number of words to copy.
 *
 *	_data		The data to copy.
 *
 * DESCRIPTION:
 *	This is a convenience macro which converts _numWords to a byte count
 *	and calls PEXCopyBytesToOC.
 */

#define PEXCopyWordsToOC(_display, _numWords, _data) \
    PEXCopyBytesToOC (_display, NUMBYTES (_numWords), _data)



/*
 * NAME:
 *	PEXInitOC
 *
 * ARGUMENTS:
 *	_display	The display pointer.
 *
 *	_resID		The resource ID of a renderer or structure.
 *
 *	_reqType	The OC request type
 *
 *	_ocHeaderLength	The number of words for the OC header.
 *
 *	_ocDataLength	The number of words for the OC data.
 *
 *	_pBuf		Return a pointer to the transport buffer
 *			which points to the start of the OC.
 *
 * DESCRIPTION:
 *	This macro will initialize an OC encoding in the X transport buffer.
 *	It will return a pointer in _pBuf which points to the start of the OC.
 *	Note that the macro does not have a "}" at the end - this is so the
 *	hidden variable 'pexDisplayInfo' can be used by other macros.  The
 *	macro 'PEXFinishOC' will have the "}".
 */

#define PEXInitOC(_display, _resID, _reqType, _ocHeaderLength, _ocDataLength, _pBuf) \
{ \
    PEXDisplayInfo 	*pexDisplayInfo; \
    int			ocLength = _ocHeaderLength + _ocDataLength; \
\
    _pBuf = NULL; \
    PEXGetDisplayInfo (display, pexDisplayInfo); \
    if (ocLength > MAX_REQUEST_SIZE) \
    { \
        _PEXGenOCBadLengthError (_display, _resID, _reqType); \
    } \
    else if (PEXStartOCs (_display, _resID, _reqType, \
	pexDisplayInfo->fpFormat, 1, ocLength)) \
    { \
    	_pBuf = display->bufptr; \
    	display->bufptr += NUMBYTES (_ocHeaderLength); \
    }



/*
 * NAME:
 *	PEXFinishOC
 *
 * ARGUMENTS:
 *	_display	The display pointer.
 *
 * DESCRIPTION:
 *	This macro is the same as the PEXFinishOCs function, but is used
 *      internally by PEXlib to avoid a function call.
 */

#define PEXFinishOC(_display) \
    UnlockDisplay (_display); \
}



/*
 * NAME:
 *	GetStringsLength
 *
 * ARGUMENTS:
 *	_numStrings	The number of strings in the mono encoded text.
 *
 *	_stringList	The list of strings in the mono encoded text.
 *
 *	_lenofStrings	Return the length of the strings.
 *
 * DESCRIPTION:
 * 	Compute length of mono-encoded strings.
 */

#define GetStringsLength(_numStrings, _stringList, _lenofStrings) \
{ \
    PEXEncodedTextData  	*nextString; \
    int				i; \
\
    (_lenofStrings) = 0; \
    nextString = (_stringList); \
    for (i = 0; i < (_numStrings); i++, nextString++) \
    { \
	lenofStrings += LENOF (pexMonoEncoding); \
	if (nextString->character_set_width == PEXCSLong) \
	    (_lenofStrings) += NUMWORDS (nextString->length * SIZEOF (long));\
	else if (nextString->character_set_width == PEXCSShort) \
	    (_lenofStrings) += NUMWORDS (nextString->length * SIZEOF (short));\
	else /* ( nextString->character_set_width == PEXCSByte) */ \
	    (_lenofStrings) += NUMWORDS (nextString->length); \
    } \
}




/* -------------------------------------------------------------------------
 * Macros for setting up Output Commands.
 * ------------------------------------------------------------------------- */

/*
 * Output Command names and opcodes.
 */

#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define OCNAME(_name_) pex##_name_
#define OCOPCODE(_name_) PEXOC##_name_
#define OCSIZE(_name_) sz_pex##_name_
#define OCLEN(_name_) ((sz_pex##_name_) >> 2)

#else
#define OCNAME(_name_) pex/**/_name_
#define OCOPCODE(_name_) PEXOC/**/_name_
#define OCSIZE(_name_) sz_pex/**/_name_
#define OCLEN(_name_) ((sz_pex/**/_name_) >> 2)
#endif


/* 
 * PEXGetOCReq sets up an OC request to be sent to the X server.  If there
 * isn't enough room left in the X buffer, it is flushed before the new
 * request is started.  Unlike PEXGetReq, display->bufptr is not updated
 * by the size of the request header at this point.  The calling routine
 * should initialize the header and bump up display->bufptr.
 */

#define PEXGetOCReq(_nBytes) \
{ \
    if ((display)->bufptr + (_nBytes) > (display)->bufmax) \
        _XFlush (display); \
    (display)->last_req = (display)->bufptr; \
    (display)->request++; \
}


/*
 * Start a new OC request header.
 */

#ifndef WORD64

#define BEGIN_NEW_OCREQ_HEADER(_pBuf, _pReq) \
    _pReq = (pexOCRequestHeader *) _pBuf;

#define END_NEW_OCREQ_HEADER(_pBuf, _pReq) 

#else /* WORD64 */

#define BEGIN_NEW_OCREQ_HEADER(_pBuf, _pReq) \
{ \
    pexOCRequestHeader tReq; \
    _pReq = &tReq;

#define END_NEW_OCREQ_HEADER(_pBuf, _pReq) \
    memcpy (_pBuf, _pReq, SIZEOF (pexOCRequestHeader)); \
}

#endif /* WORD64 */


/*
 * Update an existing OC request header.
 */

#ifndef WORD64

#define BEGIN_UPDATE_OCREQ_HEADER(_pBuf, _pReq) \
    _pReq = (pexOCRequestHeader *) _pBuf;

#define END_UPDATE_OCREQ_HEADER(_pBuf, _pReq) 

#else /* WORD64 */

#define BEGIN_UPDATE_OCREQ_HEADER(_pBuf, _pReq) \
{ \
    pexOCRequestHeader tReq; \
    _pReq = &tReq; \
    memcpy (_pReq, _pBuf, SIZEOF (pexOCRequestHeader));

#define END_UPDATE_OCREQ_HEADER(_pBuf, _pReq) \
    memcpy (_pBuf, _pReq, SIZEOF (pexOCRequestHeader)); \
}

#endif /* WORD64 */


/*
 * Begin a new OC for a Store Elements or Render Output Commands Request.
 */

#ifndef WORD64

#define BEGIN_OC_HEADER(_name, _dataLength, _pBuf, _pOC) \
    _pOC = (OCNAME(_name) *) _pBuf; \
    _pOC->oc_opcode = OCOPCODE(_name); \
    _pOC->oc_length = OCLEN(_name) + _dataLength;

#define END_OC_HEADER(_name, _pBuf, _pOC) 

#else /* WORD64 */

#define BEGIN_OC_HEADER(_name, _dataLength, _pBuf, _pOC) \
{ \
    OCNAME(_name) tReq; \
    _pOC = &tReq; \
    _pOC->oc_opcode = OCOPCODE(_name); \
    _pOC->oc_length = OCLEN(_name) + _dataLength;

#define END_OC_HEADER(_name, _pBuf, _pOC) \
    memcpy (_pBuf, _pOC, OCSIZE(_name)); \
}

#endif /* WORD64 */


/*
 * Add a simple OC to the transport buffer.
 */

#define BEGIN_SIMPLE_OC(_ocName, _resId, _reqType, _pReq) \
{ \
    char *pBuf; \
    PEXInitOC (display, _resId, _reqType, OCLEN(_ocName), 0, pBuf); \
    if (pBuf == NULL) return; \
    BEGIN_OC_HEADER (_ocName, 0, pBuf, req);

#define END_SIMPLE_OC(_ocName, _resId, _reqType, _pReq) \
    END_OC_HEADER (_ocName, pBuf, _pReq); \
    PEXFinishOC (display); \
}


/*
 * Begin a new OC for a Store Elements or Render Output Commands Request
 * which will go in an application OC encoded buffer.
 */

#ifndef WORD64

#define BEGIN_ENCODE_OCHEADER(_name, _opCode, _dataLength, _pBuf, _pOC) \
    _pOC = (OCNAME(_name) *) _pBuf; \
    _pOC->oc_opcode = _opCode; \
    _pOC->oc_length = OCLEN(_name) + _dataLength;

#define END_ENCODE_OCHEADER(_name, _pBuf, _pOC) \
    _pBuf += OCSIZE(_name);

#else /* WORD64 */

#define BEGIN_ENCODE_OCHEADER(_name, _opCode, _dataLength, _pBuf, _pOC) \
{ \
    OCNAME(_name) tReq; \
    _pOC = &tReq; \
    _pOC->oc_opcode = _opCode; \
    _pOC->oc_length = OCLEN(_name) + _dataLength;

#define END_ENCODE_OCHEADER(_name, _pBuf, _pOC) \
    memcpy (_pBuf, _pOC, OCSIZE(_name)); \
    _pBuf += OCSIZE(_name); \
}

#endif /* WORD64 */


/*
 * Encode a simple OC in an application buffer.
 */

#define BEGIN_SIMPLE_ENCODE(_name, _opCode, _pBuf, _pOC) \
    BEGIN_ENCODE_OCHEADER (_name, _opCode, 0, _pBuf, _pOC)

#define END_SIMPLE_ENCODE(_name, _pBuf, _pOC) \
    END_ENCODE_OCHEADER (_name, _pBuf, _pOC)



/*
 * Trim curve header.
 */

#ifndef WORD64

#define BEGIN_TRIMCURVE_HEAD(_pBuf, _pTrim) \
    _pTrim = (pexTrimCurve *) _pBuf;

#define END_TRIMCURVE_HEAD(_pBuf, _pTrim) 

#else /* WORD64 */

#define BEGIN_TRIMCURVE_HEAD(_pBuf, _pTrim) \
{ \
    pexTrimCurve tTrim; \
    _pTrim = &tTrim;

#define END_TRIMCURVE_HEAD(_pBuf, _pTrim) \
    memcpy (_pBuf, _pTrim, SIZEOF (pexTrimCurve)); \
}

#endif /* WORD64 */



/* -------------------------------------------------------------------------
 * Macros to compute the number of words in a facet/vertex with data.
 * ------------------------------------------------------------------------- */

/*
 * Compute the number of protocol words in the facet data
 */

#define GetFacetDataLength(_fattribs, _lenofColor) \
    (((_fattribs & PEXGAColor) ? _lenofColor : 0) + \
    ((_fattribs & PEXGANormal) ? LENOF(pexVector3D) : 0))


/*
 * Compute the number of protocol words in a vertex with
 * optional colors and normals
 */

#define GetVertexWithDataLength(_vattribs, _lenofColor) \
    (LENOF (pexCoord3D) + \
    ((_vattribs & PEXGAColor) ? _lenofColor : 0) + \
    ((_vattribs & PEXGANormal) ? LENOF (pexVector3D) : 0))


/*
 * Compute the number of bytes in the client's facet data structure
 */

#define GetClientVertexSize(_colorType, _vertexAttribs) \
    (sizeof (PEXCoord) + \
    ((_vertexAttribs & PEXGAColor) ?  GetClientColorSize (_colorType) : 0) + \
    ((_vertexAttribs & PEXGANormal) ? sizeof (PEXVector) : 0))


/*
 * Compute the number of bytes in the client's vertex data structure
 * with optional colors and normals
 */

#define GetClientFacetSize(_colorType, _facetAttribs) \
    (((_facetAttribs & PEXGAColor) ? GetClientColorSize (_colorType) : 0) + \
    ((_facetAttribs & PEXGANormal) ? sizeof (PEXVector) : 0))



/* -------------------------------------------------------------------------
 * Generic macros to store OC data.
 * ------------------------------------------------------------------------- */

#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)

#define STORE_LIST(_name, _count, _pList, _pBuf) \
    STORE_LISTOF_##_name (_count, _pList, _pBuf)

#define STORE_LISTFP(_name, _count, _pList, _pBuf, _fpConvert, _fpFormat) \
    STORE_LISTOF_##_name (_count, _pList, _pBuf, _fpConvert, _fpFormat)

#else

#define STORE_LIST(_name, _count, _pList, _pBuf) \
    STORE_LISTOF_/**/_name (_count, _pList, _pBuf)

#define STORE_LISTFP(_name, _count, _pList, _pBuf, _fpConvert, _fpFormat) \
    STORE_LISTOF_/**/_name (_count, _pList, _pBuf, _fpConvert, _fpFormat)

#endif


/*
 * OC data without floating point values.
 */

#define OC_LISTOF_FOO(_pexType, _storeMacro, _count, _fooList) \
{ \
    _pexType	*fooPtr = (_pexType *) _fooList; \
    int 	bytesLeft = _count * SIZEOF (_pexType); \
    int		maxSize, copyBytes, fooCount; \
    char	*bufPtr; \
\
    maxSize = PEXGetOCAddrMaxSize (display); \
    copyBytes = (bytesLeft < maxSize) ? \
        bytesLeft : (maxSize - maxSize % SIZEOF (_pexType)); \
    while (copyBytes > 0) \
    { \
	bufPtr = PEXGetOCAddr (display, copyBytes); \
        fooCount = copyBytes / SIZEOF (_pexType); \
	_storeMacro (fooCount, fooPtr, bufPtr); \
	fooPtr += fooCount; \
	bytesLeft -= copyBytes; \
        copyBytes = (bytesLeft < maxSize) ? \
            bytesLeft : (maxSize - maxSize % SIZEOF (_pexType)); \
    } \
}


/*
 * OC data with floating point values.
 */

#define OC_LISTOF_FOOFP(_pexType, _libType, _storeMacro, _count, _fooList, _fpConvert, _fpFormat) \
{ \
    _libType	*fooPtr = _fooList; \
    int 	bytesLeft = _count * SIZEOF (_pexType); \
    int		maxSize, copyBytes, fooCount; \
    char	*bufPtr; \
\
    maxSize = PEXGetOCAddrMaxSize (display); \
    copyBytes = (bytesLeft < maxSize) ? \
        bytesLeft : (maxSize - maxSize % SIZEOF (_pexType)); \
    while (copyBytes > 0) \
    { \
	bufPtr = PEXGetOCAddr (display, copyBytes); \
        fooCount = copyBytes / SIZEOF (_pexType); \
	_storeMacro (fooCount, fooPtr, bufPtr, _fpConvert, _fpFormat); \
	fooPtr += fooCount; \
	bytesLeft -= copyBytes; \
        copyBytes = (bytesLeft < maxSize) ? \
            bytesLeft : (maxSize - maxSize % SIZEOF (_pexType)); \
    } \
}



/* -------------------------------------------------------------------------
 * OC_LISTOF_... macros
 * ------------------------------------------------------------------------- */

#define OC_LISTOF_CARD8(_count, _pList) \
    PEXCopyBytesToOC (display, _count, (char *) _pList);


#define OC_LISTOF_CARD8_PAD(_count, _pList) \
    _PEXCopyPaddedBytesToOC (display, _count, (char *) _pList);


#ifndef WORD64

#define OC_LISTOF_CARD32(_count, _pList) \
    PEXCopyWordsToOC (display, _count, (char *) _pList);


#define OC_LISTOF_CARD16(_count, _pList) \
    PEXCopyBytesToOC (display, _count * SIZEOF (CARD16), (char *) _pList);


#define OC_LISTOF_CARD16_PAD(_count, _pList) \
    _PEXCopyPaddedBytesToOC (display, _count * SIZEOF (CARD16), \
	(char *) _pList);


#define OC_LISTOF_FLOAT32(_count, _pList, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        OC_LISTOF_FOOFP (float, float, STORE_LISTOF_FLOAT32, \
	    _count, _pList, _fpConvert, _fpFormat); \
    } \
    else \
    { \
        PEXCopyBytesToOC (display, \
           _count * SIZEOF (float), (char *) _pList);\
    }


#define OC_LISTOF_COORD4D(_count, _pList, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        OC_LISTOF_FOOFP (pexCoord4D, PEXCoord4D, STORE_LISTOF_COORD4D, \
	    _count, _pList, _fpConvert, _fpFormat); \
    } \
    else \
    { \
        PEXCopyBytesToOC (display, \
           _count * SIZEOF (pexCoord4D), (char *) _pList);\
    }


#define OC_LISTOF_COORD3D(_count, _pList, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        OC_LISTOF_FOOFP (pexCoord3D, PEXCoord, STORE_LISTOF_COORD3D, \
	    _count, _pList, _fpConvert, _fpFormat); \
    } \
    else \
    { \
        PEXCopyBytesToOC (display, \
           _count * SIZEOF (pexCoord3D), (char *) _pList);\
    }


#define OC_LISTOF_COORD2D(_count, _pList, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        OC_LISTOF_FOOFP (pexCoord2D, PEXCoord2D, STORE_LISTOF_COORD2D, \
	    _count, _pList, _fpConvert, _fpFormat); \
    } \
    else \
    { \
        PEXCopyBytesToOC (display, \
           _count * SIZEOF (pexCoord2D), (char *) _pList);\
    }


#define OC_LISTOF_HALFSPACE3D(_count, _pList, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        OC_LISTOF_FOOFP (pexHalfSpace, PEXHalfSpace, \
	    STORE_LISTOF_HALFSPACE3D, _count, _pList, _fpConvert, _fpFormat); \
    } \
    else \
    { \
        PEXCopyBytesToOC (display, \
           _count * SIZEOF (pexHalfSpace), (char *) _pList);\
    }


#define OC_LISTOF_HALFSPACE2D(_count, _pList, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        OC_LISTOF_FOOFP (pexHalfSpace2D, PEXHalfSpace2D, \
	    STORE_LISTOF_HALFSPACE2D, _count, _pList, _fpConvert, _fpFormat); \
    } \
    else \
    { \
        PEXCopyBytesToOC (display, \
           _count * SIZEOF (pexHalfSpace2D), (char *) _pList);\
    }


#else /* WORD64 */


#define OC_LISTOF_CARD32(_count, _pList) \
    OC_LISTOF_FOO (CARD32, STORE_LISTOF_CARD32, _count, _pList);


#define OC_LISTOF_CARD16(_count, _pList) \
    OC_LISTOF_FOO (CARD16, STORE_LISTOF_CARD16, _count, _pList);


#define OC_LISTOF_CARD16_PAD(_count, _pList) \
{ \
    OC_LISTOF_FOO (CARD16, STORE_LISTOF_CARD16, _count, _pList); \
    if (_count & 1) PEXGetOCAddr (display, SIZEOF (CARD16)); \
}


#define OC_LISTOF_FLOAT32(_count, _pList, _fpConvert, _fpFormat) \
    OC_LISTOF_FOOFP (float, float, STORE_LISTOF_FLOAT32, \
        _count, _pList, _fpConvert, _fpFormat);


#define OC_LISTOF_COORD4D(_count, _pList, _fpConvert, _fpFormat) \
    OC_LISTOF_FOOFP (pexCoord4D, PEXCoord4D, STORE_LISTOF_COORD4D, \
	_count, _pList, _fpConvert, _fpFormat);


#define OC_LISTOF_COORD3D(_count, _pList, _fpConvert, _fpFormat) \
    OC_LISTOF_FOOFP (pexCoord3D, PEXCoord, STORE_LISTOF_COORD3D, \
	_count, _pList, _fpConvert, _fpFormat);


#define OC_LISTOF_COORD2D(_count, _pList, _fpConvert, _fpFormat) \
    OC_LISTOF_FOOFP (pexCoord2D, PEXCoord2D, STORE_LISTOF_COORD2D, \
	_count, _pList, _fpConvert, _fpFormat);


#define OC_LISTOF_HALFSPACE3D(_count, _pList, _fpConvert, _fpFormat)\
    OC_LISTOF_FOOFP (pexHalfSpace, PEXHalfSpace, STORE_LISTOF_HALFSPACE3D, \
        _count, _pList, _fpConvert, _fpFormat);


#define OC_LISTOF_HALFSPACE2D(_count, _pList, _fpConvert, _fpFormat)\
    OC_LISTOF_FOOFP (pexHalfSpace2D, PEXHalfSpace2D, STORE_LISTOF_HALFSPACE2D,\
        _count, _pList, _fpConvert, _fpFormat);


#endif /* WORD64 */



/*
 * FACET and VERTEX data
 */


#define OC_FACET(_colorType, _facetAttr, _facetData, _fpConvert, _fpFormat)\
{ \
    if (_fpConvert) \
    { \
        _PEXOCFacet (display, _colorType, _facetAttr, _facetData, _fpFormat); \
    } \
    else \
    { \
        int numWords = GetFacetDataLength ( \
	    _facetAttr, GetColorLength (_colorType)); \
\
        PEXCopyWordsToOC (display, numWords, (char *) _facetData); \
    } \
}


#define OC_LISTOF_FACET(_count, _facetLen, _colorType, _facetAttr, _facetData, _fpConvert, _fpFormat) \
{ \
    if (_fpConvert) \
    { \
	_PEXOCListOfFacet (display, _count, _colorType, \
	    _facetAttr, _facetData, _fpFormat); \
    } \
    else \
    { \
        PEXCopyWordsToOC (display, _count * _facetLen, \
	    (char *) _facetData.index); \
    } \
}


#define OC_LISTOF_VERTEX(_count, _vertexLen, _colorType, _vertAttr, _vertData, _fpConvert, _fpFormat) \
{ \
    if (_fpConvert) \
    { \
	_PEXOCListOfVertex (display, _count, _colorType, \
	    _vertAttr, _vertData, _fpFormat); \
    } \
    else \
    { \
        PEXCopyWordsToOC (display, _count * _vertexLen, \
	    (char *) _vertData.no_data); \
    } \
}


/*
 * Color data.
 */

#define OC_LISTOF_COLOR(_count, _colorLen, _colorType, _colors, _fpConvert, _fpformat) \
{ \
    if (_fpConvert) \
    { \
        _PEXOCListOfColor (display, _count, _colorType, _colors, _fpformat); \
    } \
    else \
    { \
        PEXCopyWordsToOC (display, _count * _colorLen, \
	    (char *) _colors.indexed); \
    } \
}



/*
 * Mono encoded text string
 */

#ifndef WORD64

#define OC_MONOENCODING(_encoding) \
    PEXCopyWordsToOC (display, LENOF (pexMonoEncoding), (char *) _encoding);

#define OC_DEFAULT_MONO_STRING(_count, _string) \
{ \
    pexMonoEncoding *pMonoEncoding; \
    char	    *pBuf; \
\
    pBuf = PEXGetOCAddr (display, SIZEOF (pexMonoEncoding)); \
    pMonoEncoding = (pexMonoEncoding *) pBuf; \
    pMonoEncoding->characterSet = (INT16) 1; \
    pMonoEncoding->characterSetWidth = (CARD8) PEXCSByte; \
    pMonoEncoding->encodingState = 0;   \
    pMonoEncoding->numChars = (CARD16) (_count); \
\
    _PEXCopyPaddedBytesToOC (display, _count, _string); \
}

#else /* WORD64 */

#define OC_MONOENCODING(_encoding) \
{ \
    pexMonoEncoding tEncoding; \
    tEncoding.characterSet = (INT16) _encoding->character_set; \
    tEncoding.characterSetWidth = (CARD8) _encoding->character_set_width; \
    tEncoding.encodingState = _encoding->encoding_state;   \
    tEncoding.numChars = (CARD16) _encoding->length; \
    PEXCopyWordsToOC (display, LENOF (pexMonoEncoding), (char *) &tEncoding); \
}

#define OC_DEFAULT_MONO_STRING(_count, _string) \
{ \
    pexMonoEncoding tMonoEncoding; \
    char	    *pBuf; \
\
    pBuf = PEXGetOCAddr (display, SIZEOF (pexMonoEncoding)); \
    tMonoEncoding.characterSet = (INT16) 1; \
    tMonoEncoding.characterSetWidth = (CARD8) PEXCSByte; \
    tMonoEncoding.encodingState = 0;   \
    tMonoEncoding.numChars = (CARD16) (_count); \
    memcpy (pBuf, &tMonoEncoding, SIZEOF (pexMonoEncoding)); \
\
    _PEXCopyPaddedBytesToOC (display, _count, _string); \
}

#endif /* WORD64 */


#define OC_LISTOF_MONO_STRING(_numStrings, _stringList) \
{ \
    PEXEncodedTextData  	*nextString; \
    int				i; \
\
    nextString = (_stringList); \
    for (i = 0; i < (_numStrings); i++, nextString++) \
    { \
        OC_MONOENCODING (nextString); \
	if (nextString->character_set_width == PEXCSLong) \
	    _PEXCopyPaddedBytesToOC (display, \
		nextString->length * SIZEOF (long), \
		(char *) nextString->ch); \
	else if (nextString->character_set_width == PEXCSShort) \
	    _PEXCopyPaddedBytesToOC (display, \
		nextString->length * SIZEOF (short), \
		(char *) nextString->ch); \
	else /* nextString->character_set_width == PEXCSByte) */ \
	    _PEXCopyPaddedBytesToOC (display, \
		nextString->length, (char *) nextString->ch); \
    } \
}
