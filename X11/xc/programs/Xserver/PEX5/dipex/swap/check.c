/* $Xorg: check.c,v 1.7 2001/02/09 02:04:16 xorgcvs Exp $

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

#include "X.h"
#include "Xproto.h"
#include "PEX.h"
#include "PEXproto.h"
#include "PEXprotost.h"
#include "dipex.h"
#include "pexSwap.h"
#include "pex_site.h"
#include "pexError.h"

/*
	Check floating point format for every request and set the
	Request and OC tables appropriately.

	Note that the code depends on the fact that every request
	that is concerned with floating point format uses the 3rd short
	as a floating point specifier;
 */

extern void SwapFLOAT();
extern void SwapIEEEToVax();
extern void SwapVaxToIEEE();
extern void ConvertIEEEToVax();
extern void ConvertVaxToIEEE();
extern OCFunction cPEXOutputCmd[];
extern OCFunction uPEXOutputCmd[];
extern RequestFunction PEXRequest[];
extern RequestFunction cPEXRequest[];
extern ReplyFunction uPEXReply[];

/* define some macros previously taken from swapmacros.h */
/* byte swap AND COPY a short (given pointer) */
#define SWAPSHORTC(x, y)\
((char *) (y))[1] = ((char *) (x))[0];\
((char *) (y))[0] = ((char *) (x))[1]


typedef struct {
    CARD8 	reqType;
    CARD8 	opcode;
    CARD16 	length B16;
    INT16	fpFormat B16;
    CARD16	unused B16;
} checkStdHeader;


static PEXFLOAT
NoFloatConv( f )
PEXFLOAT f;
{
    return (f);
}

static ErrorCode
NoFloat( cntxtPtr, strmPtr )
pexContext *cntxtPtr;
checkStdHeader *strmPtr;
{
    if (cntxtPtr->client->swapped) {
	    cntxtPtr->pexRequest	=  cPEXRequest;
	    cntxtPtr->pexSwapRequestOC	=  cPEXOutputCmd; 
	    cntxtPtr->pexSwapReply	=  uPEXReply;
	    cntxtPtr->pexSwapReplyOC	=  uPEXOutputCmd;
	    cntxtPtr->swap->ConvertFLOAT=  0;
	
    } else {
	    cntxtPtr->pexRequest	=   PEXRequest;
	    cntxtPtr->pexSwapRequestOC	=   0;
	    cntxtPtr->pexSwapReply	=   0;
	    cntxtPtr->pexSwapReplyOC	=   0;
	    cntxtPtr->swap->ConvertFLOAT=   0;
	}
   return(Success);
}


/* HACK ALERT ON */
static INT16 lastfp[MAXCLIENTS];
/* HACK ALERT OFF */

static ErrorCode
CheckFloat( cntxtPtr, strmPtr )
pexContext *cntxtPtr;
checkStdHeader *strmPtr;
{
/* HACK ALERT ON */
extern INT16 lastfp[MAXCLIENTS];
/* HACK ALERT OFF */

    pexEnumTypeIndex fp;

    if (cntxtPtr->client->swapped) {
	cntxtPtr->pexRequest	    =  cPEXRequest;
	cntxtPtr->pexSwapRequestOC  =  cPEXOutputCmd; 
	cntxtPtr->pexSwapReply	    =  uPEXReply;
	cntxtPtr->pexSwapReplyOC    =  uPEXOutputCmd;
	SWAPSHORTC(&(strmPtr->fpFormat), &fp);

	if ( fp == SERVER_NATIVE_FP ) {
	    cntxtPtr->swap->ConvertFLOAT =  (ConvFunction)SwapFLOAT;

	} else {
	    if (fp == PEXDEC_F_Floating)
		cntxtPtr->swap->ConvertFLOAT = (ConvFunction)SwapVaxToIEEE;
	    else if (fp == PEXIeee_754_32)
		cntxtPtr->swap->ConvertFLOAT = (ConvFunction)SwapIEEEToVax;
	/* we don't do the other kinds yet */
	    else PEX_ERR_EXIT(PEXFloatingPointFormatError,fp,cntxtPtr);
	}

    } else {
	if ( (fp = strmPtr->fpFormat) == SERVER_NATIVE_FP ) {
	    cntxtPtr->pexRequest	=  PEXRequest;
	    cntxtPtr->pexSwapRequestOC	=  0;
	    cntxtPtr->pexSwapReply	=  0;
	    cntxtPtr->pexSwapReplyOC	=  0;
	    cntxtPtr->swap->ConvertFLOAT=  0;

	} else {
	    cntxtPtr->pexRequest	=  cPEXRequest;
	    cntxtPtr->pexSwapRequestOC	=  cPEXOutputCmd; 
	    cntxtPtr->pexSwapReply	=  uPEXReply;
	    cntxtPtr->pexSwapReplyOC	=  uPEXOutputCmd;
	    if (fp == PEXDEC_F_Floating)
		cntxtPtr->swap->ConvertFLOAT = (ConvFunction)ConvertVaxToIEEE;
	    else if (fp == PEXIeee_754_32)
		cntxtPtr->swap->ConvertFLOAT = (ConvFunction)ConvertIEEEToVax;
	/* we don't do the other kinds yet */
	    else PEX_ERR_EXIT(PEXFloatingPointFormatError,fp,cntxtPtr);
	}

    };

    /* HACK ALERT ON */
    /* set the floating point format for use by HackFloat */
    lastfp[cntxtPtr->client->index] = fp;
    /* HACK ALERT OFF */

    return(Success);
}

static ErrorCode
HackFloat( cntxtPtr, strmPtr )
pexContext *cntxtPtr;
checkStdHeader *strmPtr;
{
extern INT16 lastfp[MAXCLIENTS];

    pexEnumTypeIndex fp;

    /* This Routine is  a HACK to set the cntxtPtr up for any Requests
       that need to SWAP Floats but don't have the fpFormat as part of
       the request (ie PEXUpdatePickMeasure). Instead of getting the
       fp type from the request it will retrieve it from a global array
       which stores, on a per client basis the fpFormat of the last 
       request that had one.
    */

    fp = lastfp[cntxtPtr->client->index];
    if (cntxtPtr->client->swapped) {
	cntxtPtr->pexRequest	    =  cPEXRequest;
	cntxtPtr->pexSwapRequestOC  =  cPEXOutputCmd; 
	cntxtPtr->pexSwapReply	    =  uPEXReply;
	cntxtPtr->pexSwapReplyOC    =  uPEXOutputCmd;

	if ( fp == SERVER_NATIVE_FP ) {
	    cntxtPtr->swap->ConvertFLOAT =  (ConvFunction)SwapFLOAT;

	} else {
	    if (fp == PEXDEC_F_Floating)
		cntxtPtr->swap->ConvertFLOAT = (ConvFunction)SwapVaxToIEEE;
	    else if (fp == PEXIeee_754_32)
		cntxtPtr->swap->ConvertFLOAT = (ConvFunction)SwapIEEEToVax;
	/* we don't do the other kinds yet */
	    else PEX_ERR_EXIT(PEXFloatingPointFormatError,fp,cntxtPtr);
	}

    } else {
	if ( fp  == SERVER_NATIVE_FP ) {
	    cntxtPtr->pexRequest	=  PEXRequest;
	    cntxtPtr->pexSwapRequestOC	=  0;
	    cntxtPtr->pexSwapReply	=  0;
	    cntxtPtr->pexSwapReplyOC	=  0;
	    cntxtPtr->swap->ConvertFLOAT=  0;

	} else {
	    cntxtPtr->pexRequest	=  cPEXRequest;
	    cntxtPtr->pexSwapRequestOC	=  cPEXOutputCmd; 
	    cntxtPtr->pexSwapReply	=  uPEXReply;
	    cntxtPtr->pexSwapReplyOC	=  uPEXOutputCmd;
	    if (fp == PEXDEC_F_Floating)
		cntxtPtr->swap->ConvertFLOAT = (ConvFunction)ConvertVaxToIEEE;
	    else if (fp == PEXIeee_754_32)
		cntxtPtr->swap->ConvertFLOAT = (ConvFunction)ConvertIEEEToVax;
	/* we don't do the other kinds yet */
	    else PEX_ERR_EXIT(PEXFloatingPointFormatError,fp,cntxtPtr);
	}

    };
   return(Success);
}

RequestFunction set_tables[] = {
/*   0	*/	NoFloat,		/* PEXRequestUnused */
/*   1	*/	NoFloat,		/* PEXGetExtensionInfo */
/*   2	*/	NoFloat,		/* PEXGetEnumeratedTypeInfo */
/*   3	*/	CheckFloat,		/* PEXGetImpDepConstants */
/*   4	*/	NoFloat,		/* PEXCreateLookupTable */
/*   5	*/	NoFloat,		/* PEXCopyLookupTable */
/*   6	*/	NoFloat,		/* PEXFreeLookupTable */
/*   7	*/	NoFloat,		/* PEXGetTableInfo */
/*   8	*/	CheckFloat,		/* PEXGetPredefinedEntries */
/*   9	*/	NoFloat,		/* PEXGetDefinedIndices */
/*  10	*/	CheckFloat,		/* PEXGetTableEntry */
/*  11	*/	CheckFloat,		/* PEXGetTableEntries */
/*  12	*/	CheckFloat,		/* PEXSetTableEntries */
/*  13	*/	NoFloat,		/* PEXDeleteTableEntries */
/*  14	*/	CheckFloat,		/* PEXCreatePipelineContext */
/*  15	*/	NoFloat,		/* PEXCopyPipelineContext */
/*  16	*/	NoFloat,		/* PEXFreePipelineContext */
/*  17	*/	CheckFloat,		/* PEXGetPipelineContext */
/*  18	*/	CheckFloat,		/* PEXChangePipelineContext */
/*  19	*/	CheckFloat,		/* PEXCreateRenderer */
/*  20	*/	NoFloat,		/* PEXFreeRenderer */
/*  21	*/	CheckFloat,		/* PEXChangeRenderer */
/*  22	*/	CheckFloat,		/* PEXGetRendererAttributes */
/*  23	*/	NoFloat,		/* PEXGetRendererDynamics */
/*  24	*/	NoFloat,		/* PEXBeginRendering */
/*  25	*/	NoFloat,		/* PEXEndRendering */
/*  26	*/	NoFloat,		/* PEXBeginStructure */
/*  27	*/	NoFloat,		/* PEXEndStructure */
/*  28	*/	CheckFloat,		/* PEXRenderOutputCommands */
/*  29	*/	NoFloat,		/* PEXRenderNetwork */
/*  30	*/	NoFloat,		/* PEXCreateStructure */
/*  31	*/	NoFloat,		/* PEXCopyStructure */
/*  32	*/	NoFloat,		/* PEXDestroyStructures */
/*  33	*/	CheckFloat,		/* PEXGetStructureInfo */
/*  34	*/	CheckFloat,		/* PEXGetElementInfo */
/*  35	*/	NoFloat,		/* PEXGetStructuresInNetwork */
/*  36	*/	NoFloat,		/* PEXGetAncestors */
/*  37	*/	NoFloat,		/* PEXGetDescendants */
/*  38	*/	CheckFloat,		/* PEXFetchElements */
/*  39	*/	NoFloat,		/* PEXSetEditingMode */
/*  40	*/	NoFloat,		/* PEXSetElementPointer */
/*  41	*/	NoFloat,		/* PEXSetElementPointerAtLabel */
/*  42	*/	NoFloat,		/* PEXElementSearch */
/*  43	*/	CheckFloat,		/* PEXStoreElements */
/*  44	*/	NoFloat,		/* PEXDeleteElements */
/*  45	*/	NoFloat,		/* PEXDeleteElementsToLabel */
/*  46	*/	NoFloat,		/* PEXDeleteBetweenLabels */
/*  47	*/	NoFloat,		/* PEXCopyElements */
/*  48	*/	NoFloat,		/* PEXChangeStructureRefs */
/*  49	*/	NoFloat,		/* PEXCreateNameSet */
/*  50	*/	NoFloat,		/* PEXCopyNameSet */
/*  51	*/	NoFloat,		/* PEXFreeNameSet */
/*  52	*/	NoFloat,		/* PEXGetNameSet */
/*  53	*/	NoFloat,		/* PEXChangeNameSet */
/*  54	*/	CheckFloat,		/* PEXCreateSearchContext */
/*  55	*/	NoFloat,		/* PEXCopySearchContext */
/*  56	*/	NoFloat,		/* PEXFreeSearchContext */
/*  57	*/	CheckFloat,		/* PEXGetSearchContext */
/*  58	*/	CheckFloat,		/* PEXChangeSearchContext */
/*  59	*/	NoFloat,		/* PEXSearchNetwork */
/*  60	*/	NoFloat,		/* PEXCreatePhigsWks */
/*  61	*/	NoFloat,		/* PEXFreePhigsWks */
/*  62	*/	CheckFloat,		/* PEXGetWksInfo */
/*  63	*/	NoFloat,		/* PEXGetDynamics */
/*  64	*/	CheckFloat,		/* PEXGetViewRep */
/*  65	*/	NoFloat,		/* PEXRedrawAllStructures */
/*  66	*/	NoFloat,		/* PEXUpdateWorkstation */
/*  67	*/	NoFloat,		/* PEXRedrawClipRegion */
/*  68	*/	NoFloat,		/* PEXExecuteDeferredActions */
/*  69	*/	NoFloat,		/* PEXSetViewPriority */
/*  70	*/	NoFloat,		/* PEXSetDisplayUpdateMode */
/*  71	*/	CheckFloat,		/* PEXMapDCtoWC */
/*  72	*/	CheckFloat,		/* PEXMapWCtoDC */
/*  73	*/	CheckFloat,		/* PEXSetViewRep */
/*  74	*/	CheckFloat,		/* PEXSetWksWindow */
/*  75	*/	CheckFloat,		/* PEXSetWksViewport */
/*  76	*/	NoFloat,		/* PEXSetHlhsrMode */
/*  77	*/	NoFloat,		/* PEXSetWksBufferMode */
/*  78	*/	CheckFloat,		/* PEXPostStructure */
/*  79	*/	NoFloat,		/* PEXUnpostStructure */
/*  80	*/	NoFloat,		/* PEXUnpostAllStructures */
/*  81	*/	NoFloat,		/* PEXGetWksPostings */
/*  82	*/	CheckFloat,		/* PEXGetPickDevice */
/*  83	*/	CheckFloat,		/* PEXChangePickDevice */
/*  84	*/	NoFloat,		/* PEXCreatePickMeasure */
/*  85	*/	NoFloat,		/* PEXFreePickMeasure */
/*  86	*/	NoFloat,		/* PEXGetPickMeasure */
/*  87	*/	HackFloat,		/* PEXUpdatePickMeasure */
/*  88	*/	NoFloat,		/* PEXOpenFont */
/*  89	*/	NoFloat,		/* PEXCloseFont */
/*  90	*/	NoFloat,		/* PEXQueryFont */
/*  91	*/	NoFloat,		/* PEXListFonts */
/*  92	*/	NoFloat,		/* PEXListFontsWithInfo */
/*  93	*/	CheckFloat,		/* PEXQueryTextExtents */
/*  94	*/	NoFloat,    		/* PEXMatchRendererTargets */
/*  95	*/	NoFloat,    		/* PEXEscape */
/*  96	*/	NoFloat,    		/* PEXEscapeWithReply */
/*  97	*/	NoFloat,    		/* PEXRenderElements */
/*  98	*/	NoFloat,    		/* PEXAccumulateState */
/*  99	*/	CheckFloat,    		/* PEXBeginPickOne */
/* 100	*/	NoFloat,    		/* PEXEndPickOne */
/* 101	*/	CheckFloat,    		/* PEXPickOne */
/* 102	*/	CheckFloat,    		/* PEXBeginPickAll */
/* 103	*/	NoFloat,    		/* PEXEndPickAll */
/* 104	*/	CheckFloat    		/* PEXPickAll */
};
