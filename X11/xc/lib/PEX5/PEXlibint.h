/* $Xorg: PEXlibint.h,v 1.5 2001/02/09 02:03:26 xorgcvs Exp $ */

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

#ifndef _PEXLIBINT_H_
#define _PEXLIBINT_H_

#define NEED_REPLIES
#include <X11/Xlibint.h>
#include <X11/Xfuncs.h>
#include "PEXproto.h"
#include "pl_convert.h"
#include "pl_store.h"
#include "pl_extract.h"
#include "pl_xdata.h"


/* -------------------------------------------------------------------------
 * Display extension data structures and macros.
 * ------------------------------------------------------------------------- */

/*
 * For each display initialized by PEXInitialize(), a record is allocated
 * which holds various information about that display.  These records are
 * maintained in a linked list.  The record for the most recently referenced
 * display is always kept at the beginning of the list (for quick access).
 */

typedef struct PEXDisplayInfo
{
    Display             *display;    /* pointer to X display structure */
    XExtCodes		*extCodes;   /* extension codes */
    PEXExtensionInfo	*extInfo;    /* extension info */
    unsigned char       extOpcode;   /* opcode for pex extension */
    unsigned short      fpFormat;    /* floating point format */
    char		fpConvert;   /* flag for floating point conversion */
    PEXEnumTypeDesc	*fpSupport;  /* float formats supported by server */
    int			fpCount;     /* number of float formats supported */
    XID       		lastResID;   /* renderer/structure ID of last OC */
    int                 lastReqType; /* request type (store/rend) of last OC */
    int			lastReqNum;  /* request number of last OC */
    struct PEXDisplayInfo *next;     /* next in list */
} PEXDisplayInfo;


/*
 * Insert a new record in the beginning of the linked list.
 */

#define PEXAddDisplayInfo(_display, _info) \
\
{ \
    _info->display = _display; \
\
    _info->next = PEXDisplayInfoHeader; \
    PEXDisplayInfoHeader = _info; \
}


/*
 * Remove the record assosicated with '_display' from the linked list
 * and return a pointer to it in '_info'.
 */

#define PEXRemoveDisplayInfo(_display, _info) \
\
{ \
    PEXDisplayInfo	*prev = NULL; \
\
    _info = PEXDisplayInfoHeader; \
\
    while (_info && _info->display != _display) \
    { \
	prev = _info; \
	_info = _info->next; \
    } \
\
    if (_info) \
	if (!prev) \
	    PEXDisplayInfoHeader = _info->next; \
	else \
	    prev->next = _info->next; \
}	


/*
 * Return the info assosicated with '_display' in '_info'.
 * If the info is not the first in the list, move it to the front.
 */

#define PEXGetDisplayInfo(_display, _info) \
\
{ \
    if ((_info = PEXDisplayInfoHeader)) \
    { \
        if (PEXDisplayInfoHeader->display != _display) \
        { \
	    PEXDisplayInfo	*prev = PEXDisplayInfoHeader; \
\
	    _info = _info->next; \
	    while (_info && _info->display != _display) \
	    { \
	        prev = _info; \
	        _info = _info->next; \
	    } \
\
	    if (_info) \
	    { \
	        prev->next = _info->next; \
	        _info->next = PEXDisplayInfoHeader; \
	        PEXDisplayInfoHeader = _info; \
	    } \
	} \
    } \
}



/* -------------------------------------------------------------------------
 * Memory related macros.
 * ------------------------------------------------------------------------- */

#define PAD(_size) (3 - (((_size) + 3) & 0x3))

#define PADDED_BYTES(_bytes) (_bytes + PAD (_bytes))

#define NUMWORDS(_size) (((unsigned int) ((_size) + 3)) >> 2)

#define NUMBYTES(_len) (((unsigned int) (_len)) << 2)

#define LENOF(_ctype) (SIZEOF (_ctype) >> 2)


/* 
 * Count the number of ones in a longword.
 */

#define CountOnes(_mask, _countReturn) \
    _countReturn = ((_mask) - (((_mask)>>1)&0x77777777) \
	- (((_mask)>>2)&0x33333333) - (((_mask)>>3)&0x11111111)); \
    _countReturn = ((((_countReturn)+((_countReturn)>>4)) & 0x0F0F0F0F) % 255)



/* -------------------------------------------------------------------------
 * Macros for dealing with the transport buffer. 
 * ------------------------------------------------------------------------- */

/*
 * The maximum protocol request size.
 */

#define MAX_REQUEST_SIZE ((1<<16) - 1)


/*
 * Has the X transport buffer been flushed?
 */

#define XBufferFlushed(_display) \
    ((_display)->buffer == (_display)->bufptr)


/*
 * The number of bytes left in the X transport buffer.
 */

#define BytesLeftInXBuffer(_display) \
    ((_display)->bufmax - (_display)->bufptr)


/*
 * The number of words left in the X transport buffer.
 */

#define WordsLeftInXBuffer(_display) \
    (((_display)->bufmax - (_display)->bufptr) >> 2)


/* 
 * See if XSynchronize has been called.  If so, send request right away.
 */

#define PEXSyncHandle(_display)\
    if ((_display)->synchandler) (*(_display)->synchandler) (_display)


/*
 * Read a reply into a scratch buffer.
 */

#define XREAD_INTO_SCRATCH(_display, _pBuf, _numBytes) \
    _pBuf = (char *) _XAllocTemp (_display, (unsigned long) (_numBytes)); \
    _XRead (_display, _pBuf, (long) (_numBytes));

#define FINISH_WITH_SCRATCH(_display, _pBuf, _numBytes) \
    _XFreeTemp (_display, _pBuf, (unsigned long) (_numBytes));


/* -------------------------------------------------------------------------
 * Output Command request header.  The pexOpCode field specifies the type
 * of request - Render Output Commands or Store Elements.
 * ------------------------------------------------------------------------- */

typedef struct pexOCRequestHeader
{
    CARD8       extOpcode;
    CARD8       pexOpcode;
    CARD16      reqLength B16;
    INT16       fpFormat B16;
    CARD16      pad B16;
    CARD32      target B32;
    CARD32      numCommands B32;
} pexOCRequestHeader;



/* -------------------------------------------------------------------------
 * Macros for setting up requests.
 * ------------------------------------------------------------------------- */

/*
 * Request names and opcodes.
 */

#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define REQNAME(_name_) pex##_name_##Req
#define REQOPCODE(_name_) PEXRC##_name_
#define REQSIZE(_name_) sz_pex##_name_##Req
#else
#define REQNAME(_name_) pex/**/_name_/**/Req
#define REQOPCODE(_name_) PEXRC/**/_name_
#define REQSIZE(_name_) sz_pex/**/_name_/**/Req
#endif


/* 
 * PEXGetReq sets up a request to be sent to the X server.  If there isn't
 * enough room left in the X buffer, it is flushed before the new request
 * is started.
 */

#define PEXGetReq(_name, _req) \
    if ((display->bufptr + REQSIZE(_name)) > display->bufmax) \
        _XFlush (display); \
    _req = (char *) (display->last_req = display->bufptr); \
    display->bufptr += REQSIZE(_name); \
    display->request++


/*
 * PEXGetReqExtra is the same as PEXGetReq and except that an additional
 * "n" bytes are allocated after the request.  "n" will be padded to a word
 * boundary.
 */

#define PEXGetReqExtra(_name, _n, _req) \
    if ((display->bufptr + REQSIZE(_name) + \
	PADDED_BYTES (_n)) > display->bufmax) _XFlush (display); \
    _req = (char *) (display->last_req = display->bufptr); \
    display->bufptr += (REQSIZE(_name) + PADDED_BYTES (_n)); \
    display->request++


/*
 * BEGIN_REQUEST_HEADER and END_REQUEST_HEADER are used to hide
 * the extra work that has to be done on 64 bit clients.  On such
 * machines, all structure pointers must point to an 8 byte boundary.
 * As a result, we must first store the request header info in
 * a static data stucture, then bcopy it into the transport buffer.
 */

#ifndef WORD64

#define BEGIN_REQUEST_HEADER(_name, _pBuf, _pReq) \
{ \
    PEXDisplayInfo *pexDisplayInfo; \
    PEXGetDisplayInfo (display, pexDisplayInfo); \
    _pReq = (REQNAME(_name) *) _pBuf;

#define END_REQUEST_HEADER(_name, _pBuf, _pReq) \
    _pBuf += REQSIZE(_name); \
}

#else /* WORD64 */

#define BEGIN_REQUEST_HEADER(_name, _pBuf, _pReq) \
{ \
    PEXDisplayInfo *pexDisplayInfo; \
    REQNAME(_name) tReq; \
    PEXGetDisplayInfo (display, pexDisplayInfo); \
    _pReq = &tReq;

#define END_REQUEST_HEADER(_name, _pBuf, _pReq) \
    memcpy (_pBuf, _pReq, REQSIZE(_name)); \
    _pBuf += REQSIZE(_name); \
}

#endif /* WORD64 */


/*
 * Macros used to store the request header info.
 */

#define PEXStoreReqHead(_name, _req) \
    _req->reqType = pexDisplayInfo->extOpcode; \
    _req->opcode = REQOPCODE(_name); \
    _req->length = (REQSIZE(_name)) >> 2;


#define PEXStoreFPReqHead(_name, _fpFormat, _req) \
    _req->reqType = pexDisplayInfo->extOpcode; \
    _req->opcode = REQOPCODE(_name); \
    _req->length = (REQSIZE(_name)) >> 2; \
    _req->fpFormat = _fpFormat;

#define PEXStoreReqExtraHead(_name, _extraBytes, _req) \
    _req->reqType = pexDisplayInfo->extOpcode; \
    _req->opcode = REQOPCODE(_name); \
    _req->length = (REQSIZE(_name) + PADDED_BYTES (_extraBytes)) >> 2;

#define PEXStoreFPReqExtraHead(_name, _fpFormat, _extraBytes, _req) \
    _req->reqType = pexDisplayInfo->extOpcode; \
    _req->opcode = REQOPCODE(_name); \
    _req->length = (REQSIZE(_name) + PADDED_BYTES (_extraBytes)) >> 2; \
    _req->fpFormat = _fpFormat;



/*
 * Return flag for floating point conversion, as well as the
 * float format to convert to.  The call to this macro must come
 * after BEGIN_REQUEST_HEADER and before END_REQUEST_HEADER.
 */

#define CHECK_FP(_fpConvert, _fpFormat) \
    _fpConvert = pexDisplayInfo->fpConvert; \
    _fpFormat = pexDisplayInfo->fpFormat;



/* -------------------------------------------------------------------------
 * Get pointer to a structure in a buffer stream.  On 64 bit clients,
 * all structure pointers must point to an 8 byte boundary.  As a result,
 * we must first bcopy into a static data structure, then return a pointer
 * to this static data structure.
 *
 * Note: GET_STRUCT_PTR must be used in the declaration section of a block.
 * ------------------------------------------------------------------------- */

#ifndef WORD64

#define GET_STRUCT_PTR(_name, _pBuf, _pStruct) \
    _pStruct = (_name *) _pBuf;

#else /* WORD64 */

#define GET_STRUCT_PTR(_name, _pBuf, _pStruct) \
    _name tStruct; \
    memcpy (&tStruct, _pBuf, SIZEOF (_name)); \
    _pStruct = &tStruct;

#endif /* WORD64 */



/* -------------------------------------------------------------------------
 * Color related macros.
 * ------------------------------------------------------------------------- */

/* 
 * Protocol color size based on color type (in bytes).
 */

#define GetColorSize(_type) \
    ((_type) == PEXColorTypeIndexed ? SIZEOF (pexIndexedColor) : \
    ((_type) == PEXColorTypeRGB8 ? SIZEOF (pexRgb8Color) : \
    ((_type) == PEXColorTypeRGB16 ? SIZEOF (pexRgb16Color) : \
	SIZEOF (pexRgbFloatColor))))

/* 
 * Protocol color size based on color type (in words).
 */

#define GetColorLength(_type)\
    ((_type) == PEXColorTypeIndexed ?  LENOF (pexIndexedColor) :\
    ((_type) == PEXColorTypeRGB8 ? LENOF (pexRgb8Color) :\
    ((_type) == PEXColorTypeRGB16 ? LENOF (pexRgb16Color) : \
	LENOF (pexRgbFloatColor))))

/* 
 * Client color size based on color type (in bytes).
 */

#define GetClientColorSize(_type) \
    ((_type) == PEXColorTypeIndexed ? sizeof (PEXColorIndexed) : \
    ((_type) == PEXColorTypeRGB8 ? sizeof (PEXColorRGB8) : \
    ((_type) == PEXColorTypeRGB16 ? sizeof (PEXColorRGB16) : \
	sizeof (PEXColorRGB))))



/* -------------------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------------------- */

/*
 * IEEE-754-32 is the most common floating point type.  Vendors who have
 * a different native floating point format should define NATIVE_FP_FORMAT
 * at compile time via the -D switch (this is done by modifying the vendors
 * config file to include a "#define PexNativeFPFormat your_format".
 */

#ifndef NATIVE_FP_FORMAT
#define NATIVE_FP_FORMAT	PEXIEEE_754_32
#endif


/*
 * The PEXlib SI supports Cray floating point format, but this constant
 * is not a registered float format, so it's not found in PEX.h.  We define
 * it here.
 *
 * If a vendor wants to add support for his own float format in PEXlib, he
 * should add a consant here for the format, and bump up NUM_FP_FORMATS.
 * Then he must modify the fp conversion function table found in the file
 * pl_global_def.h to include his format.
 *
 * Note : Floating point formats 1-4 are registered with PEX at this time.
 */

#define PEXCRAY_Floating	5

#define NUM_FP_FORMATS		5


/*
 * Maximum size of pick cache in bytes.
 */

#define MAX_PICK_CACHE_SIZE	2048


/*
 * Protocol data structure sizes.  SIZEOF (rec) == sz_rec.
 */

#define sz_pexAccumulateStateReq 		12
#define sz_pexAddToNameSet 			4
#define sz_pexRemoveFromNameSet 		4
#define sz_pexAnnotationText 			32
#define sz_pexAnnotationText2D 			24
#define sz_pexApplicationData 			8
#define sz_pexATextStyle 			8
#define sz_pexBeginPickAllReq 			28
#define sz_pexBeginPickOneReq 			20
#define sz_pexBeginRenderingReq 		12
#define sz_pexBeginStructureReq 		12
#define sz_pexCellArray 			48
#define sz_pexCellArray2D 			28
#define sz_pexChangeNameSetReq 			12
#define sz_pexChangePickDeviceReq 		20
#define sz_pexChangePipelineContextReq 		24
#define sz_pexChangeRendererReq 		16
#define sz_pexChangeSearchContextReq 		16
#define sz_pexChangeStructureRefsReq 		12
#define sz_pexCharExpansion 			8
#define sz_pexCharHeight 			8
#define sz_pexATextHeight			8
#define sz_pexCharSpacing 			8
#define sz_pexCharUpVector 			12
#define sz_pexATextUpVector 			12
#define sz_pexCopyElementsReq 			36
#define sz_pexCopyLookupTableReq 		12
#define sz_pexCopyNameSetReq 			12
#define sz_pexCopyPipelineContextReq 		24
#define sz_pexCopySearchContextReq 		16
#define sz_pexCopyStructureReq 			12
#define sz_pexCreateLookupTableReq 		16
#define sz_pexCreatePickMeasureReq 		16
#define sz_pexCreatePipelineContextReq 		24
#define sz_pexCreateRendererReq 		20
#define sz_pexCreateSearchContextReq 		16
#define sz_pexCreateWorkstationReq 		76
#define sz_pexFacetCullingMode 			8
#define sz_pexCurveApprox	 		12
#define sz_pexDeleteBetweenLabelsReq 		16
#define sz_pexDeleteElementsReq 		24
#define sz_pexDeleteElementsToLabelReq 		20
#define sz_pexDeleteTableEntriesReq 		12
#define sz_pexDestroyStructuresReq 		8
#define sz_pexFacetDistinguishFlag 		8
#define sz_pexElementSearchReply 		32
#define sz_pexElementSearchReq 			28
#define sz_pexEndPickAllReply 			32
#define sz_pexEndPickAllReq 			8
#define sz_pexEndPickOneReply 			32
#define sz_pexEndPickOneReq 			8
#define sz_pexEndRenderingReq 			12
#define sz_pexEscapeReq 			8
#define sz_pexEscapeWithReplyReq 		sz_pexEscapeReq
#define sz_pexEscapeWithReplyReply 		32
#define sz_pexExecuteStructure 			8
#define sz_pexExtendedCellArray 		52
#define sz_pexFillAreaWithData 			16
#define sz_pexFillAreaSetWithData		20
#define sz_pexFetchElementsReply 		32
#define sz_pexFetchElementsReq 			28
#define sz_pexFillArea 				8
#define sz_pexFillArea2D 			8
#define sz_pexFillAreaSet 			12
#define sz_pexFillAreaSet2D 			12
#define sz_pexGDP 				16
#define sz_pexGDP2D 				16
#define sz_pexGetAncestorsReply 		32
#define sz_pexGetAncestorsReq 			16
#define sz_pexGetDescendantsReq 		sz_pexGetAncestorsReq
#define sz_pexGetDefinedIndicesReply 		32
#define sz_pexGetElementInfoReply 		32
#define sz_pexGetElementInfoReq 		28
#define sz_pexGetEnumTypeInfoReply 		32
#define sz_pexGetEnumTypeInfoReq 		16
#define sz_pexGetExtensionInfoReply 		32
#define sz_pexGetExtensionInfoReq 		8
#define sz_pexGetImpDepConstantsReply 		32
#define sz_pexGetImpDepConstantsReq 		16
#define sz_pexGetNameSetReply 			32
#define sz_pexGetPickDeviceReply 		32
#define sz_pexGetPickDeviceReq 			16
#define sz_pexGetPickMeasureReply 		32
#define sz_pexGetPickMeasureReq 		12
#define sz_pexGetPipelineContextReply 		32
#define sz_pexGetPipelineContextReq 		24
#define sz_pexGetPredefinedEntriesReply 	32
#define sz_pexGetPredefinedEntriesReq 		20
#define sz_pexGetRendererAttributesReply	32
#define sz_pexGetRendererAttributesReq 		16
#define sz_pexGetRendererDynamicsReply 		32
#define sz_pexGetSearchContextReply 		32
#define sz_pexGetSearchContextReq 		16
#define sz_pexGetStructureInfoReply 		32
#define sz_pexGetStructureInfoReq 		12
#define sz_pexGetStructuresInNetworkRepl	32
#define sz_pexGetStructuresInNetworkReq 	12
#define sz_pexGetTableEntriesReply 		32
#define sz_pexGetTableEntriesReq 		16
#define sz_pexGetTableEntryReply 		32
#define sz_pexGetTableEntryReq 			16
#define sz_pexGetTableInfoReply 		32
#define sz_pexGetTableInfoReq 			12
#define sz_pexGetWorkstationAttributesReply 	32
#define sz_pexGetWorkstationAttributesReq 	20
#define sz_pexGetWorkstationDynamicsReply 	32
#define sz_pexGetWorkstationDynamicsReq 	8
#define sz_pexGetWorkstationPostingsReply 	32
#define sz_pexGetWorkstationViewRepReply 	32
#define sz_pexGetWorkstationViewRepReq 		12
#define sz_pexGlobalTransform 			68
#define sz_pexGlobalTransform2D 		40
#define sz_pexGSE 				12
#define sz_pexHLHSRID				8
#define sz_pexInteriorStyle 			8
#define sz_pexBFInteriorStyle 			8
#define sz_pexLabel 				8
#define sz_pexLightSourceState 			8
#define sz_pexLineType 				8
#define sz_pexLineWidth 			8
#define sz_pexSurfaceEdgeWidth 			8
#define sz_pexListFontsReply 			32
#define sz_pexListFontsReq 			8
#define sz_pexListFontsWithInfoReply 		32
#define sz_pexListFontsWithInfoReq 		12
#define sz_pexLoadFontReq 			12
#define sz_pexLocalTransform 			72
#define sz_pexLocalTransform2D 			44
#define sz_pexLookupTable 			4
#define sz_pexMapDCtoWCReply 			32
#define sz_pexMapDCtoWCReq 			16
#define sz_pexMapWCtoDCReply 			32
#define sz_pexMapWCtoDCReq 			16
#define sz_pexMarkers 				4
#define sz_pexMarkers2D				4

#define sz_pexMarkerBundleIndex 		8
#define sz_pexTextBundleIndex 			8
#define sz_pexLineBundleIndex 			8
#define sz_pexInteriorBundleIndex 		8
#define sz_pexInteriorStyleIndex 		8
#define sz_pexBFInteriorStyleIndex 		8
#define sz_pexEdgeBundleIndex 			8
#define sz_pexViewIndex 			8
#define sz_pexDepthCueIndex 			8
#define sz_pexColorApproxIndex 			8

#define sz_pexMarkerColorIndex 			8
#define sz_pexTextColorIndex			8
#define sz_pexLineColorIndex			8
#define sz_pexSurfaceColorIndex			8
#define sz_pexBFSurfaceColorIndex		8
#define sz_pexSurfaceEdgeColorIndex		8

#define sz_pexMarkerColor 			8
#define sz_pexTextColor 			8
#define sz_pexLineColor 			8
#define sz_pexSurfaceColor 			8
#define sz_pexBFSurfaceColor 			8
#define sz_pexSurfaceEdgeColor 			8

#define sz_pexTextFontIndex			8

#define sz_pexMarkerScale 			8
#define sz_pexMarkerType 			8
#define sz_pexMatchRenderingTargetsReply 	32
#define sz_pexMatchRenderingTargetsReq 		20
#define sz_pexMaxHitsReachedEvent 		32
#define sz_pexModelClipFlag 			8
#define sz_pexModelClipVolume 			8
#define sz_pexModelClipVolume2D 		8
#define sz_pexNoop 				4
#define sz_pexNURBCurve 			24
#define sz_pexNURBSurface 			28
#define sz_pexParaSurfCharacteristics 		8
#define sz_pexPatternAttributes			40
#define sz_pexPatternAttributes2D		12
#define sz_pexPatternSize 			12
#define sz_pexPickAllReply 			32
#define sz_pexPickAllReq 			20
#define sz_pexPickID 				8
#define sz_pexPickOneReply 			32
#define sz_pexPickOneReq 			20
#define sz_pexPolyline 				4
#define sz_pexPolyline2D 			4
#define sz_pexPolylineInterpMethod		8
#define sz_pexPolylineSetWithData		12
#define sz_pexPostStructureReq 			20
#define sz_pexQuadrilateralMesh 		16
#define sz_pexQueryFontReply 			32
#define sz_pexQueryFontReq 			8
#define sz_pexQueryTextExtentsReply 		32
#define sz_pexQueryTextExtentsReq 		36
#define sz_pexRedrawClipRegionReq 		12
#define sz_pexRenderElementsReq 		28
#define sz_pexRenderNetworkReq 			16
#define sz_pexRenderOutputCommandsReq 		16
#define sz_pexRenderingColorModel 		8
#define sz_pexReq 				4
#define sz_pexResourceReq 			8
#define sz_pexFreeLookupTableReq 		sz_pexResourceReq
#define sz_pexGetDefinedIndicesReq 		sz_pexResourceReq
#define sz_pexFreePipelineContextReq 		sz_pexResourceReq
#define sz_pexFreeRendererReq 			sz_pexResourceReq
#define sz_pexGetRendererDynamicsReq 		sz_pexResourceReq
#define sz_pexEndStructureReq 			sz_pexResourceReq
#define sz_pexCreateStructureReq 		sz_pexResourceReq
#define sz_pexCreateNameSetReq 			sz_pexResourceReq
#define sz_pexFreeNameSetReq 			sz_pexResourceReq
#define sz_pexGetNameSetReq 			sz_pexResourceReq
#define sz_pexFreeSearchContextReq 		sz_pexResourceReq
#define sz_pexFreeWorkstationReq 		sz_pexResourceReq
#define sz_pexRedrawAllStructuresReq 		sz_pexResourceReq
#define sz_pexUpdateWorkstationReq 		sz_pexResourceReq
#define sz_pexExecuteDeferredActionsReq 	sz_pexResourceReq
#define sz_pexUnpostAllStructuresReq 		sz_pexResourceReq
#define sz_pexGetWorkstationPostingsReq 	sz_pexResourceReq
#define sz_pexFreePickMeasureReq 		sz_pexResourceReq
#define sz_pexUnloadFontReq 			sz_pexResourceReq
#define sz_pexSearchNetworkReq 			sz_pexResourceReq
#define sz_pexRestoreModelClipVolume		4
#define sz_pexSetOfFillAreaSets			24
#define sz_pexSearchNetworkReply 		32
#define sz_pexIndividualASF			12
#define sz_pexSetEditingModeReq 		12
#define sz_pexSetElementPointerAtLabelReq 	16
#define sz_pexSetElementPointerReq 		16
#define sz_pexSetTableEntriesReq 		16
#define sz_pexSetWorkstationBufferModeReq 	12
#define sz_pexSetWorkstationDisplayUpdateModeReq 12
#define sz_pexSetWorkstationHLHSRModeReq 	12
#define sz_pexSetWorkstationViewPriorityReq 	16
#define sz_pexSetWorkstationViewRepReq 		172
#define sz_pexSetWorkstationViewportReq 	32
#define sz_pexSetWorkstationWindowReq 		36
#define sz_pexStoreElementsReq 			16
#define sz_pexSurfaceApprox	 		16
#define sz_pexSurfaceEdgeFlag 			8
#define sz_pexSurfaceEdgeType 			8
#define sz_pexSurfaceInterpMethod		8
#define sz_pexBFSurfaceInterpMethod		8
#define sz_pexReflectionAttributes		28
#define sz_pexBFReflectionAttributes		28
#define sz_pexReflectionModel 			8
#define sz_pexBFReflectionModel 		8
#define sz_pexText 				44
#define sz_pexText2D 				16
#define sz_pexTextAlignment 			8
#define sz_pexATextAlignment 			8
#define sz_pexTextPath 				8
#define sz_pexATextPath 			8
#define sz_pexTextPrecision 			8
#define sz_pexTriangleStrip 			16
#define sz_pexUnpostStructureReq 		12
#define sz_pexUpdatePickMeasureReq 		12
#define sz_pexCieColor 				12
#define sz_pexColor 				12
#define sz_pexColorApproxEntry 			40
#define sz_pexColorSpecifier 			4
#define sz_pexColorEntry			sz_pexColorSpecifier
#define sz_pexCoord2D 				8
#define sz_pexCoord3D 				12
#define sz_pexCoord4D 				16
#define sz_pexCullMode 				2
#define sz_pexCurveApproxData 			8
#define sz_pexDepthCueEntry 			24
#define sz_pexDeviceCoord 			8
#define sz_pexDeviceCoord2D 			4
#define sz_pexDeviceRect 			8
#define sz_pexEdgeBundleEntry 			12
#define sz_pexElementInfo 			4
#define sz_pexElementPos 			8
#define sz_pexElementRange 			16
#define sz_pexElementRef 			8
#define sz_pexEnumTypeDesc 			4
#define sz_pexEnumTypeIndex 			4
#define sz_pexEscapeSetEchoColorData 		8
#define sz_pexExtentInfo 			24
#define sz_pexFloatColor 			12
#define sz_pexFont 				4
#define sz_pexFontInfo 				20
#define sz_pexFontProp 				8
#define sz_pexHalfSpace 			24
#define sz_pexHalfSpace2D 			16
#define sz_pexHlsColor 				12
#define sz_pexHsvColor 				12
#define sz_pexIndexedColor 			4
#define sz_pexInteriorBundleEntry 		28
#define sz_pexLightEntry 			48
#define sz_pexLineBundleEntry 			20
#define sz_pexLocalTransform2DData 		40
#define sz_pexLocalTransform3DData 		68
#define sz_pexMarkerBundleEntry 		12
#define sz_pexMatrix 				64
#define sz_pexMatrix3X3 			36
#define sz_pexMonoEncoding 			8
#define sz_pexName 				4
#define sz_pexNameSet 				4
#define sz_pexNameSetPair 			8
#define sz_pexNpcSubvolume 			24
#define sz_pexPD_NPC_HitVolume 			sz_pexNpcSubvolume
#define sz_pexOutputCommandError 		32
#define sz_pexPD_DC_HitBox 			8
#define sz_pexPSC_IsoparametricCurves 		8
#define sz_pexPSC_LevelCurves 			28
#define sz_pexPatternEntry 			8
#define sz_pexPickElementRef 			12
#define sz_pexPickRecord 			4
#define sz_pexReflectionAttr 			24
#define sz_pexRendererTarget 			8
#define sz_pexRgb16Color 			8
#define sz_pexRgb8Color 			4
#define sz_pexRgbFloatColor 			12
#define sz_pexString 				2
#define sz_pexStructure 			4
#define sz_pexStructureInfo 			8
#define sz_pexSurfaceApproxData			12
#define sz_pexSwitch 				1
#define sz_pexTableInfo 			8
#define sz_pexTableIndex 			2
#define sz_pexTextAlignmentData 		4
#define sz_pexTextBundleEntry 			16
#define sz_pexTextFontEntry 			4
#define sz_pexTrimCurve 			28
#define sz_pexVector2D 				8
#define sz_pexVector3D 				12
#define sz_pexVertex 				12
#define sz_pexViewEntry 			156
#define sz_pexViewRep 				160
#define sz_pexViewport 				20

/* from PEXlibint.h */
#define sz_pexOCRequestHeader 			16

/* for X-Window system protocol elements */
#define sz_CARD32 				4
#define sz_CARD16 				2
#define sz_CARD8 				1
#define sz_INT32 				4
#define sz_INT16 				2

/* for other things */
#define sz_char 				1
#define sz_short				2
#define sz_long					4
#define sz_float				4



/* -------------------------------------------------------------------------
 * Externally defined globals.
 * ------------------------------------------------------------------------- */

/*
 * Linked list of open displays.
 */

extern PEXDisplayInfo	*PEXDisplayInfoHeader;


/*
 * Pick path cache.
 */

extern PEXPickPath	*PEXPickCache;
extern unsigned int	PEXPickCacheSize;
extern int		PEXPickCacheInUse;


/*
 * Floating point conversion function table.
 */

extern void (*(PEX_fp_convert[NUM_FP_FORMATS][NUM_FP_FORMATS]))();



/* -------------------------------------------------------------------------
 * Function prototypes for PEXlib internal functions.
 * ------------------------------------------------------------------------- */

extern void _PEXCopyPaddedBytesToOC(
#if NeedFunctionPrototypes
    Display *			/* display */,
    int 			/* numBytes */,
    char *			/* data */
#endif
);

extern void _PEXSendBytesToOC(
#if NeedFunctionPrototypes
    Display *			/* display */,
    int 			/* numBytes */,
    char *			/* data */
#endif
);

extern void _PEXOCFacet(
#if NeedFunctionPrototypes
    Display *			/* display */,
    int				/* colorType */,
    unsigned int		/* facetAttr */,
    PEXFacetData *		/* facetData */,
    int				/* fpFormat */
#endif
);

extern void _PEXOCListOfFacet(
#if NeedFunctionPrototypes
    Display *			/* display */,
    int				/* count */,
    int				/* colorType */,
    unsigned int		/* facetAttr */,
    PEXArrayOfFacetData 	/* facetData */,
    int				/* fpFormat */
#endif
);

extern void _PEXOCListOfVertex(
#if NeedFunctionPrototypes
    Display *			/* display */,
    int				/* count */,
    int				/* colorType */,
    unsigned int		/* vertAttr */,
    PEXArrayOfVertex	 	/* vertData */,
    int				/* fpFormat */
#endif
);

extern void _PEXOCListOfColor(
#if NeedFunctionPrototypes
    Display *			/* display */,
    int				/* count */,
    int				/* colorType */,
    PEXArrayOfColor	 	/* colors */,
    int				/* fpFormat */
#endif
);

extern void _PEXStoreFacet(
#if NeedFunctionPrototypes
    int				/* colorType */,
    unsigned int		/* facetAttr */,
    PEXFacetData *		/* facetData */,
    char **			/* bufPtr */,
    int				/* fpFormat */
#endif
);

extern void _PEXStoreListOfFacet(
#if NeedFunctionPrototypes
    int				/* count */,
    int				/* colorType */,
    unsigned int		/* facetAttr */,
    PEXArrayOfFacetData 	/* facetData */,
    char **			/* bufPtr */,
    int				/* fpFormat */
#endif
);

extern void _PEXStoreListOfVertex(
#if NeedFunctionPrototypes
    int				/* count */,
    int				/* colorType */,
    unsigned int		/* vertAttr */,
    PEXArrayOfVertex	 	/* vertData */,
    char **			/* bufPtr */,
    int				/* fpFormat */
#endif
);

extern void _PEXStoreListOfColor(
#if NeedFunctionPrototypes
    int				/* count */,
    int				/* colorType */,
    PEXArrayOfColor	 	/* colors */,
    char **			/* bufPtr */,
    int				/* fpFormat */
#endif
);

extern void _PEXExtractFacet(
#if NeedFunctionPrototypes
    char **			/* bufPtr */,
    int				/* colorType */,
    unsigned int		/* facetAttr */,
    PEXFacetData *		/* facetData */,
    int				/* fpFormat */
#endif
);

extern void _PEXExtractListOfFacet(
#if NeedFunctionPrototypes
    int				/* count */,
    char **			/* bufPtr */,
    int				/* colorType */,
    unsigned int		/* facetAttr */,
    PEXArrayOfFacetData 	/* facetData */,
    int				/* fpFormat */
#endif
);

extern void _PEXExtractListOfVertex(
#if NeedFunctionPrototypes
    int				/* count */,
    char **			/* bufPtr */,
    int				/* colorType */,
    unsigned int		/* vertAttr */,
    PEXArrayOfVertex	 	/* vertData */,
    int				/* fpFormat */
#endif
);

extern void _PEXExtractListOfColor(
#if NeedFunctionPrototypes
    int				/* count */,
    char **			/* bufPtr */,
    int				/* colorType */,
    PEXArrayOfColor	 	/* colors */,
    int				/* fpFormat */
#endif
);

extern void _PEXGenOCBadLengthError(
#if NeedFunctionPrototypes
    Display *			/* display */,
    XID				/* resource_id */,
    PEXOCRequestType		/* req_type */
#endif
);



/* -------------------------------------------------------------------------
 * Miscellaneous.
 * ------------------------------------------------------------------------- */

/*
 * Argument types in function definitions.
 */

#define INPUT  
#define OUTPUT  
#define INOUT


/*
 * Xlib defines min and max as macros; Must undef since min and max
 * are field names in PEXlib data structures.
 */

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#endif /* _PEXLIBINT_H_ */


