/* $Xorg: PEXproto.h,v 1.4 2001/02/09 02:03:26 xorgcvs Exp $ */
/*

Copyright 1992, 1998  The Open Group

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

*/

/******************************************************************************
Copyright 1989, 1990, 1991 by Sun Microsystems, Inc.

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and that
the name of Sun Microsystems not be used in advertising or publicity
pertaining to distribution of the software without specific, written prior
permission.  Sun Microsystems makes no representations about the
suitability of this software for any purpose.  It is provided "as is" without
express or implied warranty.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************************/

/* Definitions for the PEX used by server and c bindings */

/*
 * This packet-construction scheme makes the following assumptions:
 *
 * 1. The compiler is able to generate code which addresses one- and two-byte
 * quantities.  In the worst case, this would be done with bit-fields.  If 
 * bit-fields are used it may be necessary to reorder the request fields in
 * this file, depending on the order in which the machine assigns bit fields
 * to machine words.  There may also be a problem with sign extension, as K+R 
 * specify that bitfields are always unsigned.
 *
 * 2. 2- and 4-byte fields in packet structures must be ordered by hand such 
 * that they are naturally-aligned, so that no compiler will ever insert 
 * padding bytes.
 *
 * 3. All packets are hand-padded to a multiple of 4 bytes, for the same reason.
 */

#ifndef _PEXPROTO_H_
#define _PEXPROTO_H_

/* In the following typedefs, comments appear that say "LISTof Foo( numItems )",
 * "CLIST of Foo()", and "SINGLE Foo()".   These are used when the protocol 
 * specifies that a request or reply contains a variable length list of 
 * (possibly variable types of) objects.
 *
 * A LISTof list is one for which we have already been given the length.
 * The items in the list are of type "Foo". The number of items in the list
 * appears parenthetically after the type.  ("numItems" in our example.)
 * Any other information needed to parse the list is also passed in the
 * parentheses. (E.g., "tableType" in a list of table entries.)
 *
 * A CLISTof list is the same, except that the first 4 bytes of the list
 * indicate the number of items in the list.  The length may need to be
 * byte-swapped.
 *
 * A SINGLE item of an indeterminate length is indicated in the same
 * manner.  (E.g., a "SINGLE TableEntry()".) Any other information
 * needed to parse the item is also passed in the parentheses.
 * (E.g., "itemMask" in a set of pipeline context attributes.)
 *
 * If no information is given in the parentheses, then the size is
 * implicit.
 *
 * Variable length padding is noted with a comment, with the number
 * of bytes of padding required as calculated from the value in
 * the parentheses.  (number of bytes of padding = n?(3-((n-1)%4):0 , where
 * n is the parenthetical value.)
 */

#define XID CARD32
#define Drawable CARD32

#include <X11/PEX5/PEXprotost.h>

/* Matches revision 5.1C */

/****************************************************************
 *  		REPLIES 					*
 ****************************************************************/
typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD16	majorVersion B16;
    CARD16	minorVersion B16;
    CARD32	release B32;
    CARD32	lengthName B32;
    CARD32	subsetInfo B32;
    BYTE	pad[8];
    /* LISTof CARD8 follows -- Don't swap */
    /* pad */
    } pexGetExtensionInfoReply;


typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* NOT 0; this is an extra-large reply*/
    CARD32	numLists B32;
    BYTE	pad[20];		/* lists of lists begin afterwards */
    /* LISTof CLISTof pexEnumTypeDesc( numLists ) */
    /* pad */
    } pexGetEnumTypeInfoReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    BYTE	pad[24];
    /* LISTof VALUE() */
    } pexGetImpDepConstantsReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		
    CARD32	escapeID B32;
    CARD8	escape_specific[20];
    /* more escape specific data, treat as */
    /* LISTof CARD8( length ) */
    } pexEscapeWithReplyReply;


typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		
    BYTE	pad[24];
    /* LISTof RENDERER_TARGET( ) */
    } pexMatchRenderingTargetsReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* 0 */
    CARD16	unused B16;
    CARD16	definableEntries B16;
    CARD16	numPredefined B16;
    CARD16	predefinedMin B16;
    CARD16	predefinedMax B16;
    BYTE	pad[14];
    } pexGetTableInfoReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	unused B32;
    CARD32	numEntries B32;
    BYTE	pad[16];
    /* LISTof TableEntry( numEntries, tableType ) */
    } pexGetPredefinedEntriesReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	numIndices B32;
    BYTE	pad[20];
    /* LISTof pexTableIndex( numIndices ) */
    /* pad( numIndices ) */
    } pexGetDefinedIndicesReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD16	status B16;
    CARD16	tableType B16;
    BYTE	pad[20];
    /* SINGLE TableEntry( tableType )  */
    } pexGetTableEntryReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD16	tableType B16;
    CARD16	unused B16;
    CARD32	numEntries B32;
    BYTE	pad[16];
    /* LISTof TableEntry( numEntries, tableType ) */
    } pexGetTableEntriesReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    BYTE	pad[24];
    /* SINGLE PipelineContextAttributes( itemMask )  */
    } pexGetPipelineContextReply;


typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    BYTE	pad[24];
    /* SINGLE RendererAttributes( itemMask ) */
    } pexGetRendererAttributesReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* 0 */
    pexBitmask	tables B32;
    pexBitmask	namesets B32;
    pexBitmask	attributes B32;
    BYTE	pad[12];
} pexGetRendererDynamicsReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* 0 */
    CARD16	editMode B16;
    CARD16	unused	B16;
    CARD32	elementPtr B32;
    CARD32	numElements B32;
    CARD32	lengthStructure B32;
    CARD16	hasRefs B16;
    BYTE	pad[6];
    } pexGetStructureInfoReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	numInfo B32;
    BYTE	pad[20];
    /* LISTof pexElementInfo( numInfo ) */
    } pexGetElementInfoReply;

typedef struct {
    BYTE		type;			/* X_Reply */
    CARD8		what;			/* unused */
    CARD16		sequenceNumber	B16;
    CARD32		length B32;		/* not 0 */
    CARD8		unused[8];
    CARD32		numStructures B32;
    BYTE		pad[12];
    /* LISTof pexStructure( numStructures )  */
    } pexGetStructuresInNetworkReply;

typedef struct {
    BYTE		type;			/* X_Reply */
    CARD8		what;			/* unused */
    CARD16		sequenceNumber	B16;
    CARD32		length B32;		/* not 0 */
    CARD8		unused[12];
    CARD32		numPaths B32;
    BYTE		pad[8];
    /* LISTof CLISTof pexElementRef( numPaths ) */
    } pexGetAncestorsReply;

typedef pexGetAncestorsReply pexGetDescendantsReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	numElements B32;
    BYTE	pad[20];
    /* LISTof OutputCommand( numElements ) */
    } pexFetchElementsReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* 0 */
    CARD16	status B16;
    CARD16	unused B16;
    CARD32	foundOffset B32;
    BYTE	pad[16];
    } pexElementSearchReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	numNames B32;
    BYTE	pad[20];
    /* LISTof pexName( numNames ) */
    } pexGetNameSetReply;


typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    BYTE	pad[24];
    /* SINGLE SearchContextAttributes( itemMask ) */
    } pexGetSearchContextReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	unused B32;
    CARD32	numItems B32;
    BYTE	pad[16];
    /* LISTof pexElementRef( numItems ) */
    } pexSearchNetworkReply;


typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    BYTE	pad[24];
    /* SINGLE WksInfo( itemMask ) */
    } pexGetWorkstationAttributesReply;


typedef struct {
    BYTE		type;			/* X_Reply */
    CARD8		what;			/* unused */
    CARD16		sequenceNumber	B16;
    CARD32		length B32;		/* 0 */
    pexDynamicType	viewRep;
    pexDynamicType	markerBundle;
    pexDynamicType	textBundle;
    pexDynamicType	lineBundle;
    pexDynamicType	interiorBundle;
    pexDynamicType	edgeBundle;
    pexDynamicType	colorTable;
    pexDynamicType	patternTable;
    pexDynamicType	wksTransform;
    pexDynamicType	highlightFilter;
    pexDynamicType	invisibilityFilter;
    pexDynamicType	HlhsrMode;
    pexDynamicType	structureModify;
    pexDynamicType	postStructure;
    pexDynamicType	unpostStructure;
    pexDynamicType	deleteStructure;
    pexDynamicType	referenceModify;
    pexDynamicType	bufferModify;
    pexDynamicType	lightTable;
    pexDynamicType	depthCueTable;
    pexDynamicType	colorApproxTable;
    CARD8		pad[3];
    } pexGetWorkstationDynamicsReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* 4 + 76*fp/4 */
    CARD16	viewUpdate B16;		/* Pending, NotPending */ 
    BYTE	pad[22];
    /* SINGLE pexViewRep() 	requested */
    /* SINGLE pexViewRep() 	current */
    } pexGetWorkstationViewRepReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD16	viewIndex B16;
    CARD16	unused B16;
    CARD32	numCoords B32;
    BYTE	pad[16];
    /* LISTof pexCoord3D( numCoords ) */
    } pexMapDCtoWCReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	unused B32;
    CARD32	numCoords B32;
    BYTE	pad[16];
    /* LISTof pexDeviceCoord( numCoords ) */
    } pexMapWCtoDCReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    BYTE	pad[24];
    /* LISTof pexPhigsWksID() */
    } pexGetWorkstationPostingsReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    BYTE	pad[24];
    /* SINGLE PickDeviceAttributes( itemMask ) */
    } pexGetPickDeviceReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    BYTE	pad[24];
    /* SINGLE pexPickMeasureAttributes( itemMask ) */
    } pexGetPickMeasureReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	numPickElRefs B32;	
    pexEnumTypeIndex	pickStatus B16;
    CARD8	betterPick;
    BYTE	pad[17];
    /* LISTof pexPickElementRef ( numPickElRefs ) */
    } pexEndPickOneReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	numPickElRefs B32;	
    pexEnumTypeIndex	pickStatus B16;
    CARD8	betterPick;
    BYTE	pad[17];
    /* LISTof pexPickElementRef ( numPickElRefs ) */
    } pexPickOneReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	numPicked     B32;	
    pexEnumTypeIndex	pickStatus B16;
    pexEnumTypeIndex	morePicks  B16;
    BYTE	pad[16];
    /* LISTof CLISTof pexPickElementRef ( numPicked ) */
    } pexEndPickAllReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	numPicked     B32;	
    pexEnumTypeIndex	pickStatus B16;
    pexEnumTypeIndex	morePicks  B16;
    BYTE	pad[16];
    /* LISTof CLISTof pexPickElementRef ( numPicked ) */
    } pexPickAllReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	lengthFontInfo B32;
    CARD8	pad[20];
    /* SINGLE pexFontInfo() */
    } pexQueryFontReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	numStrings B32;
    BYTE	pad[20];
    /* LISTof pexString( numStrings ) */
    } pexListFontsReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    CARD32	numStrings B32;
    BYTE	pad[20];
    /* LISTof pexString( numStrings ) */
    /* CLISTof pexFontInfo() */
    } pexListFontsWithInfoReply;

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	length B32;		/* not 0 */
    BYTE	pad[24];
    /* LISTof ExtentInfo() */
    } pexQueryTextExtentsReply;

/****************************************************************
 *  		REQUESTS 					*
 ****************************************************************/
/* Request structure */

typedef struct {
    CARD8	reqType;
    CARD8	opcode;		/* meaning depends on request type */
    CARD16	length B16;        
				/* length in 4 bytes quantities */
                                /* of whole request, including this header */
} pexReq;

/*****************************************************************
 *  structures that follow request.
 *****************************************************************/

/* ResourceReq is used for any request which has a resource ID
   ( or Atom or Time ) as its one and only argument.  */

typedef struct {
    CARD8	reqType;
    CARD8	opcode;
    CARD16	length B16;	/* 2 */
    CARD32	id B32;		/* a Structure, Renderer, Font, LUT, etc. */
    } pexResourceReq;


/*****************************************************************
 *  Specific Requests 
 *****************************************************************/


typedef struct {
    CARD8	reqType;
    CARD8	opcode;
    CARD16	length B16;	/* 2 */
    CARD16	clientProtocolMajor B16;
    CARD16	clientProtocolMinor B16;
} pexGetExtensionInfoReq;

typedef struct {
    CARD8	reqType;
    CARD8 	opcode;
    CARD16 	length B16;
    Drawable 	drawable B32;
    pexBitmask	itemMask B32;
    CARD32	numEnums B32;
    /* LISTof CARD16( numEnums ) */
    /* pad( numEnums*2 ) */
} pexGetEnumTypeInfoReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    Drawable		drawable B32;
    CARD32		numNames B32;
    /* LISTof pexImpDepConstantNames ( numNames )  */
    /* pad */
} pexGetImpDepConstantsReq;

typedef struct {
    CARD8		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 5 */
    Drawable		drawable B32;
    CARD8 		depth;
    CARD8 		unused;
    CARD16		type B16;
    CARD32		visualID B32;
    CARD32		maxTriplets B32;
} pexMatchRenderingTargetsReq;

typedef struct {
    CARD8		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 2 + n */
    CARD32		escapeID B32;
    /* 4n bytes of additional escape data to skip */
} pexEscapeReq;

typedef  pexEscapeReq   pexEscapeWithReplyReq;

typedef struct {
    CARD8		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 4 */
    Drawable		drawableExample B32;
    pexLookupTable	lut B32;
    pexTableType	tableType B16;
    CARD16		unused B16;
} pexCreateLookupTableReq;


typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;	/* 3 */
    pexLookupTable	src B32;
    pexLookupTable	dst B32;
} pexCopyLookupTableReq;

typedef pexResourceReq pexFreeLookupTableReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    Drawable		drawableExample B32;
    pexTableType	tableType B16;
    CARD16		unused B16;
} pexGetTableInfoReq;


typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 5 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    Drawable		drawableExample B32;
    pexTableType	tableType B16;
    pexTableIndex	start B16;
    CARD16		count B16;
    CARD16		pad B16;
} pexGetPredefinedEntriesReq;

typedef pexResourceReq pexGetDefinedIndicesReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 4 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		valueType B16;
    pexLookupTable	lut B32;
    pexTableIndex	index B16;
    CARD16		pad B16;
} pexGetTableEntryReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 4 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		valueType B16;
    pexLookupTable	lut B32;
    pexTableIndex	start B16;
    CARD16		count B16;
} pexGetTableEntriesReq;


typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexLookupTable	lut B32;
    pexTableIndex	start B16;
    CARD16		count B16;
/*    LISTof TableEntry( count ) */
} pexSetTableEntriesReq;


typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexLookupTable	lut B32;
    pexTableIndex	start B16;
    CARD16		count B16;
} pexDeleteTableEntriesReq;


typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;	/* 6 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexPC		pc B32;
    pexBitmask		itemMask0 B32;
    pexBitmask		itemMask1 B32;
    pexBitmask		itemMask2 B32;
    /* SINGLE PipelineContextAttributes( itemMask ) */
} pexCreatePipelineContextReq;


typedef struct {
    CARD8 	reqType;
    CARD8 	opcode;
    CARD16 	length B16;	/* 6 */
    pexPC	src B32;
    pexPC	dst B32;
    pexBitmask	itemMask0 B32;
    pexBitmask	itemMask1 B32;
    pexBitmask	itemMask2 B32;
} pexCopyPipelineContextReq;

typedef pexResourceReq  pexFreePipelineContextReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;	/* 6 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexPC		pc B32;
    pexBitmask		itemMask0 B32;
    pexBitmask		itemMask1 B32;
    pexBitmask		itemMask2 B32;
} pexGetPipelineContextReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexPC		pc B32;
    pexBitmask		itemMask0 B32;
    pexBitmask		itemMask1 B32;
    pexBitmask		itemMask2 B32;
    /* SINGLE PipelineContextAttributes( itemMask ) */
} pexChangePipelineContextReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexRenderer		rdr B32;
    Drawable		drawable B32;
    pexBitmask		itemMask B32;
    /* SINGLE RendererAttributes( itemMask ) */
} pexCreateRendererReq;

typedef pexResourceReq pexFreeRendererReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexRenderer		rdr B32;
    pexBitmask		itemMask B32;
    /* SINGLE RendererAttributes( itemMask ) */
} pexChangeRendererReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;	/* 4 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexRenderer		rdr B32;
    pexBitmask		itemMask B32;
} pexGetRendererAttributesReq;

typedef pexResourceReq pexGetRendererDynamicsReq;

typedef struct {
    CARD8 	reqType;
    CARD8 	opcode;
    CARD16 	length B16;	/* 3 */
    pexRenderer	rdr B32;
    Drawable	drawable B32;
} pexBeginRenderingReq;

typedef struct {
    CARD8 	reqType;
    CARD8 	opcode;
    CARD16 	length B16;	/* 3 */
    pexRenderer	rdr B32;
    pexSwitch	flushFlag;
    BYTE	pad[3];
} pexEndRenderingReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexRenderer		rdr B32;
    pexStructure	sid B32;
} pexBeginStructureReq;

typedef pexResourceReq pexEndStructureReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexRenderer		rdr B32;
    CARD32		numCommands B32;
    /* LISTof OutputCommand( numCommands ) */
} pexRenderOutputCommandsReq;
/* individual output commands may be found in the section "Output Commands" */


typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 7 */
    pexRenderer		rdr B32;
    pexStructure	sid B32;
    CARD16		position1_whence B16;
    CARD16		unused1 B16;
    INT32		position1_offset B32;
    CARD16		position2_whence B16;
    CARD16		unused2 B16;
    INT32		position2_offset B32;
} pexRenderElementsReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 + 2n */
    pexRenderer		rdr B32;
    CARD32              numElRefs B32;
    /* LISTof pexElementRef( numElRefs ) */
} pexAccumulateStateReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 4 */
    pexRenderer		rdr B32;
    Drawable		drawable B32;
    pexStructure	sid B32;
} pexRenderNetworkReq;

typedef pexResourceReq pexCreateStructureReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexStructure	src B32;
    pexStructure	dst B32;
} pexCopyStructureReq;

typedef struct {
    CARD8 	reqType;
    CARD8 	opcode;
    CARD16 	length B16;
    CARD32	numStructures B32;
    /* LISTof pexStructure( numStructures ) */
} pexDestroyStructuresReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		itemMask B16;
    pexStructure	sid B32;
} pexGetStructureInfoReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 7 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexStructure	sid B32;
    CARD16		position1_whence B16;
    CARD16		unused1 B16;
    INT32		position1_offset B32;
    CARD16		position2_whence B16;
    CARD16		unused2 B16;
    INT32		position2_offset B32;
} pexGetElementInfoReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexStructure	sid B32;
    CARD16		which B16;
    CARD16		pad B16;
} pexGetStructuresInNetworkReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 4 */
    pexStructure	sid B32;
    CARD16		pathOrder B16;
    CARD16		unused B16;
    CARD32		pathDepth B32;
} pexGetAncestorsReq;

typedef pexGetAncestorsReq pexGetDescendantsReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 7 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexStructure	sid B32;
    CARD16		position1_whence B16;
    CARD16		unused1 B16;
    INT32		position1_offset B32;
    CARD16		position2_whence B16;
    CARD16		unused2 B16;
    INT32		position2_offset B32;
} pexFetchElementsReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexStructure	sid B32;
    CARD16		mode B16;
    CARD16		pad B16;
} pexSetEditingModeReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 4 */
    pexStructure	sid B32;
    CARD16		position_whence B16;
    CARD16		unused B16;
    INT32		position_offset B32;
} pexSetElementPointerReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 4 */
    pexStructure	sid B32;
    INT32		label B32;
    INT32		offset B32;
} pexSetElementPointerAtLabelReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;
    pexStructure	sid B32;
    CARD16		position_whence B16;
    CARD16		unused B16;
    INT32		position_offset B32;
    CARD32		direction B32;
    CARD32		numIncls B32;
    CARD32		numExcls B32;
    /* LISTof CARD16( numIncls ) */
    /* pad( numIncls*2 ) */
    /* LISTof CARD16( numExcls ) */
    /* pad( numExcls*2 ) */
} pexElementSearchReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexStructure	sid B32;
    CARD32		numCommands B32;
    /* LISTof OutputCommand( numCommands ) */
} pexStoreElementsReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 6 */
    pexStructure	sid B32;
    CARD16		position1_whence B16;
    CARD16		unused1 B16;
    INT32		position1_offset B32;
    CARD16		position2_whence B16;
    CARD16		unused2 B16;
    INT32		position2_offset B32;
} pexDeleteElementsReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 5 */
    pexStructure	sid B32;
    CARD16		position_whence B16;
    CARD16		unused B16;
    INT32		position_offset B32;
    INT32		label B32;
} pexDeleteElementsToLabelReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 4 */
    pexStructure	sid B32;
    INT32		label1 B32;
    INT32		label2 B32;
} pexDeleteBetweenLabelsReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 9 */
    pexStructure	src B32;
    CARD16		srcPosition1_whence B16;
    CARD16		unused1 B16;
    INT32		srcPosition1_offset B32;
    CARD16		srcPosition2_whence B16;
    CARD16		unused2 B16;
    INT32		srcPosition2_offset B32;
    pexStructure	dst B32;
    CARD16		dstPosition_whence B16;
    CARD16		unused3 B16;
    INT32		dstPosition_offset B32;
} pexCopyElementsReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexStructure	old_id B32;
    pexStructure	new_id B32;
} pexChangeStructureRefsReq;

typedef pexResourceReq pexCreateNameSetReq;

typedef struct {
    CARD8 	reqType;
    CARD8 	opcode;
    CARD16 	length B16;		/* 3 */
    pexNameSet	src B32;
    pexNameSet	dst B32;
} pexCopyNameSetReq;

typedef pexResourceReq pexFreeNameSetReq;

typedef pexResourceReq pexGetNameSetReq;

typedef struct {
    CARD8 	reqType;
    CARD8 	opcode;
    CARD16 	length B16;
    pexNameSet	ns B32;
    CARD16	action B16;
    CARD16	unused B16;
    /* LISTof pexName() */
} pexChangeNameSetReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexSC		sc B32;
    pexBitmask		itemMask B32;
    /* SINGLE SearchContextAttributes( itemMask ) */
} pexCreateSearchContextReq;

typedef struct {
    CARD8 	reqType;
    CARD8 	opcode;
    CARD16 	length B16;		/* 4 */
    pexSC	src B32;
    pexSC	dst B32;
    pexBitmask	itemMask B32;
} pexCopySearchContextReq;

typedef pexResourceReq pexFreeSearchContextReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;	/* 4 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexSC		sc B32;
    pexBitmask		itemMask B32;
} pexGetSearchContextReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexSC		sc B32;
    pexBitmask		itemMask B32;
    /* SINGLE SearchContextAttributes( itemMask ) */
} pexChangeSearchContextReq;

typedef pexResourceReq pexSearchNetworkReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 19 */
    pexPhigsWks		wks B32;
    Drawable		drawable B32;
    pexLookupTable	markerBundle B32;
    pexLookupTable	textBundle B32;
    pexLookupTable	lineBundle B32;
    pexLookupTable	interiorBundle B32;
    pexLookupTable	edgeBundle B32;
    pexLookupTable	colorTable B32;
    pexLookupTable	depthCueTable B32;
    pexLookupTable	lightTable B32;
    pexLookupTable	colorApproxTable B32;
    pexLookupTable	patternTable B32;
    pexLookupTable	textFontTable B32;
    pexNameSet		highlightIncl B32;
    pexNameSet		highlightExcl B32;
    pexNameSet		invisIncl B32;
    pexNameSet		invisExcl B32;
    CARD16		bufferMode B16;
    CARD16		pad B16;
} pexCreateWorkstationReq;

typedef pexResourceReq pexFreeWorkstationReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;	/* 5 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexPhigsWks		wks B32;
    pexBitmask		itemMask0 B32;
    pexBitmask		itemMask1 B32;
} pexGetWorkstationAttributesReq;

typedef struct {
    CARD8 	reqType;
    CARD8 	opcode;
    CARD16 	length B16;		/* 2 */
    Drawable	drawable B32;
} pexGetWorkstationDynamicsReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexEnumTypeIndex	fpFormat B16;
    pexTableIndex	index B16;
    pexPhigsWks		wks B32;
} pexGetWorkstationViewRepReq;

typedef pexResourceReq pexRedrawAllStructuresReq;	

typedef pexResourceReq pexUpdateWorkstationReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;
    pexPhigsWks		wks B32;
    CARD32		numRects B32;
    /* LISTof pexDeviceRect( numRects ) */
} pexRedrawClipRegionReq;

typedef pexResourceReq pexExecuteDeferredActionsReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 4 */
    pexPhigsWks		wks B32;
    pexTableIndex	index1 B16;
    pexTableIndex	index2 B16;
    CARD16		priority B16;
    CARD16		pad B16;
} pexSetWorkstationViewPriorityReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexPhigsWks		wks B32;
    pexEnumTypeIndex	displayUpdate B16;
    CARD16		pad B16;
} pexSetWorkstationDisplayUpdateModeReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexPhigsWks		wks B32;
    CARD32		numCoords B32;
    /* LISTof pexDeviceCoord( numCoords ) */
} pexMapDCtoWCReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		index B16;
    pexPhigsWks		wks B32;
    CARD32		numCoords B32;
    /* LISTof pexCoord3D( numCoords ) */
} pexMapWCtoDCReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;	/* 43 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused1 B16;
    pexPhigsWks		wks B32;
    pexTableIndex	index B16;
    CARD16		unused2 B16;
    CARD16		clipFlags B16;
    CARD16		unused3 B16;
    PEXFLOAT		clipLimits_xmin B32;
    PEXFLOAT		clipLimits_ymin B32;
    PEXFLOAT		clipLimits_zmin B32;
    PEXFLOAT		clipLimits_xmax B32;
    PEXFLOAT		clipLimits_ymax B32;
    PEXFLOAT		clipLimits_zmax B32;
    pexMatrix		view_orientation;
    pexMatrix		view_mapping;
} pexSetWorkstationViewRepReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 9 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexPhigsWks		wks B32;
    PEXFLOAT		npcSubvolume_xmin B32;
    PEXFLOAT		npcSubvolume_ymin B32;
    PEXFLOAT		npcSubvolume_zmin B32;
    PEXFLOAT		npcSubvolume_xmax B32;
    PEXFLOAT		npcSubvolume_ymax B32;
    PEXFLOAT		npcSubvolume_zmax B32;
} pexSetWorkstationWindowReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;	/* 8 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexPhigsWks		wks B32;
    INT16		viewport_xmin B16;
    INT16		viewport_ymin B16;
    PEXFLOAT		viewport_zmin B32;
    INT16		viewport_xmax B16;
    INT16		viewport_ymax B16;
    PEXFLOAT		viewport_zmax B32;
    pexSwitch		useDrawable;
    BYTE		pad[3];
} pexSetWorkstationViewportReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexPhigsWks		wks B32;
    pexEnumTypeIndex	mode B16;
    CARD16		pad B16;
} pexSetWorkstationHLHSRModeReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;	/* 3 */
    pexPhigsWks		wks B32;
    CARD16		bufferMode B16;
    CARD16		pad B16;
} pexSetWorkstationBufferModeReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 5 */
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexPhigsWks		wks B32;
    pexStructure	sid B32;
    PEXFLOAT		priority B32;
} pexPostStructureReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexPhigsWks		wks B32;
    pexStructure	sid B32;
} pexUnpostStructureReq;

typedef pexResourceReq pexUnpostAllStructuresReq;

typedef pexResourceReq pexGetWorkstationPostingsReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 4 */
    pexEnumTypeIndex	fpFormat B16;
    pexEnumTypeIndex	devType B16;
    pexPhigsWks		wks B32;
    pexBitmask		itemMask B32;
} pexGetPickDeviceReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;
    pexEnumTypeIndex	fpFormat B16;
    CARD16		unused B16;
    pexPhigsWks		wks B32;
    pexEnumTypeIndex	devType B16;
    CARD16		unused2 B16;
    pexBitmask		itemMask B32;
    /* SINGLE PickDeviceAttributes( itemMask ) */
} pexChangePickDeviceReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 4 */
    pexPhigsWks		wks B32;
    pexPickMeasure	pm B32;
    pexEnumTypeIndex	devType B16;
    CARD16      	pad B16;
} pexCreatePickMeasureReq;

typedef pexResourceReq pexFreePickMeasureReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;	/* 3 */
    pexPickMeasure	pm B32;
    pexBitmask		itemMask B32;
} pexGetPickMeasureReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;
    pexPickMeasure	pm B32;
    CARD32		numBytes B32;
    /* LISTof CARD8( numBytes ) */
    /* pad( numBytes ) */
} pexUpdatePickMeasureReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;   /* 6 + n */
    pexEnumTypeIndex	fpFormat B16;
    pexEnumTypeIndex	method B16;
    pexRenderer		rdr B32;
    Drawable		drawable B32;
    INT32 		sid B32;
    /* SINGLE PickRecord () */
} pexBeginPickOneReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;   /* 2 */
    pexRenderer		rdr B32;
} pexEndPickOneReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;   /* 6 + n */
    pexEnumTypeIndex	fpFormat B16;
    pexEnumTypeIndex	method B16;
    pexRenderer		rdr B32;
    Drawable		drawable B32;
    pexStructure	sid B32;
    /* SINGLE PickRecord () */
} pexPickOneReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;   /* 7 + n */
    pexEnumTypeIndex	fpFormat B16;
    pexEnumTypeIndex	method B16;
    CARD8 		sendEvent;
    CARD8 		unused[3];
    pexRenderer		rdr B32;
    Drawable		drawable B32;
    INT32		sid B32;
    CARD32              pickMaxHits B32;
    /* SINGLE PickRecord () */
} pexBeginPickAllReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;   /* 2 */
    pexRenderer		rdr B32;
} pexEndPickAllReq;

typedef struct {
    CARD8 		reqType;
    CARD8 		opcode;
    CARD16 		length B16;   /* 6 + n */
    pexEnumTypeIndex	fpFormat B16;
    pexEnumTypeIndex	method B16;
    pexRenderer		rdr B32;
    Drawable		drawable B32;
    CARD32              pickMaxHits B32;
    /* SINGLE RendererPickRecord () */
} pexPickAllReq;

typedef struct {
    CARD8 	reqType;
    CARD8 	opcode;
    CARD16 	length B16;
    pexFont	font B32;
    CARD32	numBytes B32;
    /* LISTof CARD8( numBytes ) -- don't swap */
    /* pad( numBytes ) */
} pexLoadFontReq;

typedef pexResourceReq pexUnloadFontReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;
    pexFont		font B32;
} pexQueryFontReq;

typedef struct {
    CARD8 	reqType;
    CARD8 	opcode;
    CARD16 	length B16;
    CARD16	maxNames B16;
    CARD16	numChars B16;
    /* LISTof CARD8( numChars ) -- don't swap */
    /* pad( numBytes ) */
} pexListFontsReq;

typedef struct {
    CARD8		reqType;
    CARD8		opcode;
    CARD16		length B16;
    CARD16		unused B16;
    CARD16		maxNames B16;
    CARD16		numChars B16;
    CARD16		pad B16;
    /* LISTof CARD8( numChars )  */
    /* pad( numBytes ) */
} pexListFontsWithInfoReq;

typedef struct {
    CARD8		    reqType;
    CARD8		    opcode;
    CARD16		    length B16;
    pexEnumTypeIndex	    fpFormat B16;
    CARD16		    textPath B16;
    pexTableIndex	    fontGroupIndex  B16;
    CARD16		    unused B16;
    XID			    id B32;	/* renderer, wks, or text font lut */
    PEXFLOAT		    charExpansion B32;
    PEXFLOAT		    charSpacing B32;
    PEXFLOAT		    charHeight B32;
    CARD16		    alignment_vertical B16;
    CARD16		    alignment_horizontal B16;
    CARD32		    numStrings B32;
    /* LISTof LISTof MONO_ENCODINGS() */
    /* pad() */
}  pexQueryTextExtentsReq;

/*****************************************************************
 * Output Commands 
 *****************************************************************/

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	markerType B16;
    CARD16		pad B16;
} pexMarkerType;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		scale B32;
} pexMarkerScale;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexTableIndex	index B16;
    CARD16		pad B16;
} pexMarkerBundleIndex;

typedef pexMarkerBundleIndex  pexMarkerColorIndex;
typedef pexMarkerBundleIndex  pexTextColorIndex;
typedef pexMarkerBundleIndex  pexLineColorIndex;
typedef pexMarkerBundleIndex  pexSurfaceColorIndex;
typedef pexMarkerBundleIndex  pexBFSurfaceColorIndex;
typedef pexMarkerBundleIndex  pexSurfaceEdgeColorIndex;

typedef pexMarkerBundleIndex pexTextFontIndex;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexColorType	colorType B16;
    CARD16		unused B16;
    /* SINGLE COLOR(colorType) */
} pexMarkerColor;

typedef pexMarkerColor pexTextColor;
typedef pexMarkerColor pexLineColor;
typedef pexMarkerColor pexSurfaceColor;
typedef pexMarkerColor pexBFSurfaceColor;
typedef pexMarkerColor pexSurfaceEdgeColor;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	style B16;
    CARD16		pad B16;
} pexATextStyle;

typedef pexMarkerBundleIndex pexTextBundleIndex;
typedef pexMarkerBundleIndex pexLineBundleIndex;
typedef pexMarkerBundleIndex pexInteriorBundleIndex;
typedef pexMarkerBundleIndex pexInteriorStyleIndex;
typedef pexMarkerBundleIndex pexBFInteriorStyleIndex;
typedef pexMarkerBundleIndex pexEdgeBundleIndex;
typedef pexMarkerBundleIndex pexViewIndex;
typedef pexMarkerBundleIndex pexDepthCueIndex;
typedef pexMarkerBundleIndex pexColorApproxIndex;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		precision B16;
    CARD16		pad B16;
} pexTextPrecision;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		expansion B32;
} pexCharExpansion;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		spacing B32;
} pexCharSpacing;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		height B32;
} pexCharHeight;
typedef pexCharHeight pexATextHeight;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		up_x B32;
    PEXFLOAT		up_y B32;
} pexCharUpVector;
typedef pexCharUpVector pexATextUpVector;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		path B16;
    CARD16		pad B16;
} pexTextPath;
typedef pexTextPath pexATextPath;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		alignment_vertical B16;
    CARD16		alignment_horizontal B16;
} pexTextAlignment;
typedef pexTextAlignment pexATextAlignment;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	lineType B16;
    CARD16		pad B16;
} pexLineType;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		width B32;
} pexLineWidth;
typedef pexLineWidth	pexSurfaceEdgeWidth;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	approxMethod B16;
    CARD16		unused B16;
    PEXFLOAT		tolerance B32;
} pexCurveApprox;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	polylineInterp B16;
    CARD16		pad B16;
} pexPolylineInterpMethod;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	interiorStyle B16;
    CARD16		pad B16;
} pexInteriorStyle;
typedef pexInteriorStyle pexBFInteriorStyle;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		ambient B32;
    PEXFLOAT		diffuse B32;
    PEXFLOAT		specular B32;
    PEXFLOAT		specularConc B32;
    PEXFLOAT		transmission B32;
    pexColorType	specular_colorType B16;
    CARD16		unused B16;
    /* SINGLE COLOR(specular_colorType) */
} pexReflectionAttributes;
typedef pexReflectionAttributes pexBFReflectionAttributes;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	reflectionModel B16;
    CARD16		pad B16;
} pexReflectionModel;
typedef pexReflectionModel pexBFReflectionModel;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	surfaceInterp B16;
    CARD16		pad B16;
} pexSurfaceInterpMethod;
typedef pexSurfaceInterpMethod pexBFSurfaceInterpMethod;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	approxMethod B16;
    CARD16		unused B16;
    PEXFLOAT		uTolerance B32;
    PEXFLOAT		vTolerance B32;
} pexSurfaceApprox;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexCullMode		cullMode B16;
    CARD16		pad B16;
} pexFacetCullingMode;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexSwitch		distinguish;
    BYTE		pad[3];
} pexFacetDistinguishFlag;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		size_x B32;
    PEXFLOAT		size_y B32;
} pexPatternSize;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		point_x B32;
    PEXFLOAT		point_y B32;
} pexPatternAttributes2D;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		refPt_x B32;
    PEXFLOAT		refPt_y B32;
    PEXFLOAT		refPt_z B32;
    PEXFLOAT		vector1_x B32;
    PEXFLOAT		vector1_y B32;
    PEXFLOAT		vector1_z B32;
    PEXFLOAT		vector2_x B32;
    PEXFLOAT		vector2_y B32;
    PEXFLOAT		vector2_z B32;
} pexPatternAttributes;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexSwitch		onoff;
    BYTE		pad[3];
} pexSurfaceEdgeFlag;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	edgeType B16;
    CARD16		pad B16;
} pexSurfaceEdgeType;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexAsfAttribute	attribute B32;
    pexAsfValue		source;
    BYTE		pad[3];
} pexIndividualASF;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexComposition	compType B16;
    CARD16		unused B16;
    pexMatrix		matrix;
} pexLocalTransform;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexComposition	compType B16;
    CARD16		unused B16;
    pexMatrix3X3	matrix3X3;
} pexLocalTransform2D;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexMatrix		matrix;
} pexGlobalTransform;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexMatrix3X3	matrix3X3;
} pexGlobalTransform2D;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexSwitch		onoff;
    BYTE		pad[3];
} pexModelClipFlag;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	modelClipOperator B16;
    CARD16		numHalfSpaces B16;
    /* LISTof pexHalfSpace( numHalfSpaces ) */
} pexModelClipVolume;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	modelClipOperator B16;
    CARD16		numHalfSpaces B16;
    /* LISTof pexHalfSpace2D( numHalfSpaces ) */
} pexModelClipVolume2D;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
} pexRestoreModelClipVolume;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		numEnable B16;
    CARD16		numDisable B16;
    /* LISTof pexTableIndex( numEnable ) */
    /* pad( ( numEnable )*2 ) */
    /* LISTof pexTableIndex( numDisable ) */
    /* pad( ( numDisable )*2 ) */
} pexLightSourceState;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD32		pickId B32;
} pexPickID;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD32		hlhsrID B32;
} pexHLHSRID;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	model B16;
    CARD16		pad B16;
} pexRenderingColorModel;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexEnumTypeIndex	characteristics B16;
    CARD16		length B16;
    /* SINGLEof PARAMETRIC_SURFACE_CHARACTERISTICS */
} pexParaSurfCharacteristics;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    /* LISTof pexName() */
} pexAddToNameSet;
typedef pexAddToNameSet pexRemoveFromNameSet;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexStructure	id B32;
} pexExecuteStructure;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    INT32		label B32;
} pexLabel;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		numElements B16;
    CARD16		unused B16;
    /* LISTof CARD8( numElements ) -- don't swap */
    /* pad( numElements ) */
} pexApplicationData;
    
typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD32		id B32;
    CARD16		numElements B16;
    CARD16		unused B16;
    /* LISTof CARD8( numElements ) -- don't swap */
    /* pad( numElements ) */
} pexGSE;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    /* LISTof pexCoord3D() */
} pexMarkers;	

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    /* LISTof pexCoord2D() */
} pexMarkers2D;	

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		origin_x B32;
    PEXFLOAT		origin_y B32;
    PEXFLOAT		origin_z B32;
    PEXFLOAT		vector1_x B32;
    PEXFLOAT		vector1_y B32;
    PEXFLOAT		vector1_z B32;
    PEXFLOAT		vector2_x B32;
    PEXFLOAT		vector2_y B32;
    PEXFLOAT		vector2_z B32;
    CARD16		numEncodings B16;
    CARD16		unused B16;
    /* LISTof pexMonoEncoding( numEncodings ) */
} pexText;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		origin_x B32;
    PEXFLOAT		origin_y B32;
    CARD16		numEncodings B16;
    CARD16		unused B16;
    /* LISTof pexMonoEncoding( numEncodings ) */
} pexText2D;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		origin_x B32;
    PEXFLOAT		origin_y B32;
    PEXFLOAT		origin_z B32;
    PEXFLOAT		offset_x B32;
    PEXFLOAT		offset_y B32;
    PEXFLOAT		offset_z B32;
    CARD16		numEncodings B16;
    CARD16		unused B16;
    /* LISTof pexMonoEncoding( numEncodings ) */
} pexAnnotationText;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		origin_x B32;
    PEXFLOAT		origin_y B32;
    PEXFLOAT		offset_x B32;
    PEXFLOAT		offset_y B32;
    CARD16		numEncodings B16;
    CARD16		unused B16;
    /* LISTof pexMonoEncoding( numEncodings ) */
} pexAnnotationText2D;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    /* LISTof pexCoord3D() */
} pexPolyline;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    /* LISTof pexCoord2D() */
} pexPolyline2D;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexColorType	colorType B16;
    pexBitmaskShort	vertexAttribs B16;
    CARD32		numLists B32;
    /* LISTof CLISTof pexVertex( numLists, vertexAttribs, colorType ) */
} pexPolylineSetWithData;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		curveOrder B16;
    pexCoordType	coordType B16;
    PEXFLOAT		tmin B32;
    PEXFLOAT		tmax B32;
    CARD32		numKnots B32;
    CARD32		numPoints B32;
    /* LISTof FLOAT( numKnots ) */
    /* LISTof {pexCoord3D|pexCoord4D}( numPoints, coordType ) */
} pexNURBCurve;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		shape B16;
    pexSwitch		ignoreEdges;
    CARD8		pad;
    /* LISTof pexCoord3D() */
} pexFillArea;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		shape B16;
    pexSwitch		ignoreEdges;
    CARD8		unused;
    /* LISTof pexCoord2D() */
} pexFillArea2D;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		shape B16;
    pexSwitch		ignoreEdges;
    CARD8		unused;
    pexColorType	colorType B16;
    pexBitmaskShort	facetAttribs B16;
    pexBitmaskShort	vertexAttribs B16;
    CARD16		unused2 B16;
    /* SINGLE Facet( facetAttribs, vertexAttribs, colorType ) */
} pexFillAreaWithData;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		shape B16;
    pexSwitch		ignoreEdges;
    CARD8		contourHint;
    CARD32		numLists B32;
    /* LISTof CLISTof Coord3D( numLists ) */
} pexFillAreaSet;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		shape B16;
    pexSwitch		ignoreEdges;
    CARD8		contourHint;
    CARD32		numLists B32;
    /* LISTof CLISTof Coord2D( numLists ) */
} pexFillAreaSet2D;


typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		shape B16;
    pexSwitch		ignoreEdges;
    CARD8		contourHint;
    pexColorType	colorType B16;
    pexBitmaskShort	facetAttribs B16;
    pexBitmaskShort	vertexAttribs B16;
    CARD16		unused2 B16;
    CARD32		numLists B32;
    /* pexOptData( facetAttribs ) */
    /* LISTof CLISTof  pexVertex( numLists, vertexAttribs, colorType ) */
} pexFillAreaSetWithData;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexColorType	colorType B16;
    pexBitmaskShort	facetAttribs B16;
    pexBitmaskShort	vertexAttribs B16;
    CARD16		unused B16;
    CARD32		numVertices B32;
    /* number of OptData is numVert - 2 */
    /* LISTof pexOptData( facetAttribs, colorType ) */
    /* LISTof pexVertex( numVertices, vertexAttribs, colorType ) */
} pexTriangleStrip;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexColorType	colorType B16;
    CARD16		mPts B16;
    CARD16		nPts B16;
    pexBitmaskShort	facetAttribs B16;
    pexBitmaskShort	vertexAttribs B16;
    CARD16		shape B16;
    /* actually, there are (mPts-1)*(nPts-1) opt data entries */
    /* LISTof pexOptData( facetAttribs, colorType ) */
    /* LISTof pexVertex( mPts, nPts, vertexAttribs, colorType ) */
} pexQuadrilateralMesh;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    CARD16		shape B16;
    pexColorType	colorType B16;
    CARD16		FAS_Attributes B16;
    CARD16		vertexAttributes B16;
    CARD16		edgeAttributes B16;
    CARD8		contourHint;
    pexSwitch		contourCountsFlag;
    CARD16		numFAS B16;
    CARD16		numVertices B16;
    CARD16		numEdges B16;
    CARD16		numContours B16;
    /* LISTof OPT_DATA( numFAS ) */
    /* LISTof pexVertex( numVertices ) */
    /* LISTof CARD8( numEdges ) */
    /* pad( numEdges ) */
    /* LISTof CLISTof CLISTof CARD16( numFAS, numContours, numEdges ) */
    /* pad */
} pexSetOfFillAreaSets;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexCoordType 	type B16;
    CARD16		uOrder B16;
    CARD16		vOrder B16;
    CARD16		unused B16;
    CARD32		numUknots B32;
    CARD32		numVknots B32;
    CARD16		mPts B16;
    CARD16		nPts B16;
    CARD32		numLists B32;
    /* LISTof FLOAT( numUknots ) */
    /* LISTof FLOAT( numVKnots ) */
    /* LISTof {pexCoord3D|pexCoord4D}( mPts, nPts, surfaceType ) */
    /* LISTof CLISTof pexTrimCurve( numLists ) */
} pexNURBSurface;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		point1_x B32;
    PEXFLOAT		point1_y B32;
    PEXFLOAT		point1_z B32;
    PEXFLOAT		point2_x B32;
    PEXFLOAT		point2_y B32;
    PEXFLOAT		point2_z B32;
    PEXFLOAT		point3_x B32;
    PEXFLOAT		point3_y B32;
    PEXFLOAT		point3_z B32;
    CARD32		dx B32;
    CARD32		dy B32;
    /* LISTof pexTableIndex( dx, dy ) */
    /* pad(  2*dx*dy ) */
} pexCellArray;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    PEXFLOAT		point1_x B32;
    PEXFLOAT		point1_y B32;
    PEXFLOAT		point2_x B32;
    PEXFLOAT		point2_y B32;
    CARD32		dx B32;
    CARD32		dy B32;
    /* LISTof pexTableIndex( dx, dy ) */
    /* pad( 2*dx*dy ) */
} pexCellArray2D;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    pexColorType	colorType B16;
    CARD16		unused B16;
    PEXFLOAT		point1_x B32;
    PEXFLOAT		point1_y B32;
    PEXFLOAT		point1_z B32;
    PEXFLOAT		point2_x B32;
    PEXFLOAT		point2_y B32;
    PEXFLOAT		point2_z B32;
    PEXFLOAT		point3_x B32;
    PEXFLOAT		point3_y B32;
    PEXFLOAT		point3_z B32;
    CARD32		dx B32;
    CARD32		dy B32;
    /* LISTof pexColorSpecifier( dx, dy ) */
} pexExtendedCellArray;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    INT32		gdpId B32;
    CARD32		numPoints B32;
    CARD32		numBytes B32;
    /* LISTof pexCoord3D( numPoints ) */
    /* LISTof CARD8( numBytes ) -- don't swap */
    /* pad( numBytes ) */
} pexGDP;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
    INT32		gdpId B32;
    CARD32		numPoints B32;
    CARD32		numBytes B32;
    /* LISTof pexCoord2D( numPoints ) */
    /* LISTof CARD8( numBytes ) -- don't swap */
    /* pad( numBytes ) */
} pexGDP2D;

typedef struct {
    CARD16		oc_opcode B16;
    CARD16		oc_length B16;
} pexNoop;

/****************************************************************
 *  		EVENTS 						*
 ****************************************************************/
/* Event structure */

typedef struct {
    BYTE	type;			/* X_Event */
    CARD8	what;			/* unused */
    CARD16	sequenceNumber	B16;
    CARD32	rdr B32;		
    BYTE	pad[24];
} pexMaxHitsReachedEvent;

#undef XID
#undef Drawable

#endif /* _PEXPROTO_H_ */

