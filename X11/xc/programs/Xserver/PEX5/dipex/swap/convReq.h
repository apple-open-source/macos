/* $Xorg: convReq.h,v 1.4 2001/02/09 02:04:17 xorgcvs Exp $ */

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

LOCAL_FLAG ErrorCode
	SWAP_FUNC_PREFIX(PEXRequestUnused)(),
	SWAP_FUNC_PREFIX(PEXGenericRequest) (),
	SWAP_FUNC_PREFIX(PEXGenericResourceRequest) (),
	SWAP_FUNC_PREFIX(PEXGetExtensionInfo) (),
	SWAP_FUNC_PREFIX(PEXGetEnumeratedTypeInfo) (),
	SWAP_FUNC_PREFIX(PEXGetImpDepConstants) (),
	SWAP_FUNC_PREFIX(PEXCreateLookupTable) (),
	SWAP_FUNC_PREFIX(PEXCopyLookupTable) (),
	SWAP_FUNC_PREFIX(PEXGetTableInfo) (),
	SWAP_FUNC_PREFIX(PEXGetPredefinedEntries) (),
	SWAP_FUNC_PREFIX(PEXGetTableEntry) (),
	SWAP_FUNC_PREFIX(PEXGetTableEntries) (),
	SWAP_FUNC_PREFIX(PEXSetTableEntries) (),
	SWAP_FUNC_PREFIX(PEXDeleteTableEntries) (),
	SWAP_FUNC_PREFIX(PEXCreatePipelineContext) (),
	SWAP_FUNC_PREFIX(PEXCopyPipelineContext) (),
	SWAP_FUNC_PREFIX(PEXGetPipelineContext) (),
	SWAP_FUNC_PREFIX(PEXChangePipelineContext) (),
	SWAP_FUNC_PREFIX(PEXCreateRenderer) (),
	SWAP_FUNC_PREFIX(PEXChangeRenderer) (),
	SWAP_FUNC_PREFIX(PEXGetRendererAttributes) (),
	SWAP_FUNC_PREFIX(PEXBeginRendering) (),
	SWAP_FUNC_PREFIX(PEXBeginStructure) (),
	SWAP_FUNC_PREFIX(PEXRenderOutputCommands) (),
	SWAP_FUNC_PREFIX(PEXRenderNetwork) (),
	SWAP_FUNC_PREFIX(PEXCopyStructure) (),
	SWAP_FUNC_PREFIX(PEXDestroyStructures) (),
	SWAP_FUNC_PREFIX(PEXGetStructureInfo) (),
	SWAP_FUNC_PREFIX(PEXGetElementInfo) (),
	SWAP_FUNC_PREFIX(PEXGetStructuresInNetwork) (),
	SWAP_FUNC_PREFIX(PEXGetAncestors) (),
	SWAP_FUNC_PREFIX(PEXFetchElements) (),
	SWAP_FUNC_PREFIX(PEXSetEditingMode) (),
	SWAP_FUNC_PREFIX(PEXSetElementPointer) (),
	SWAP_FUNC_PREFIX(PEXSetElementPointerAtLabel) (),
	SWAP_FUNC_PREFIX(PEXElementSearch) (),
	SWAP_FUNC_PREFIX(PEXStoreElements) (),
	SWAP_FUNC_PREFIX(PEXDeleteElements) (),
	SWAP_FUNC_PREFIX(PEXDeleteElementsToLabel) (),
	SWAP_FUNC_PREFIX(PEXDeleteBetweenLabels) (),
	SWAP_FUNC_PREFIX(PEXCopyElements) (),
	SWAP_FUNC_PREFIX(PEXChangeStructureRefs) (),
	SWAP_FUNC_PREFIX(PEXCopyNameSet) (),
	SWAP_FUNC_PREFIX(PEXChangeNameSet) (),
	SWAP_FUNC_PREFIX(PEXCreateSearchContext) (),
	SWAP_FUNC_PREFIX(PEXCopySearchContext) (),
	SWAP_FUNC_PREFIX(PEXGetSearchContext) (),
	SWAP_FUNC_PREFIX(PEXChangeSearchContext) (),
	SWAP_FUNC_PREFIX(PEXCreatePhigsWks) (),
	SWAP_FUNC_PREFIX(PEXGetWksInfo) (),
	SWAP_FUNC_PREFIX(PEXGetDynamics) (),
	SWAP_FUNC_PREFIX(PEXGetViewRep) (),
	SWAP_FUNC_PREFIX(PEXRedrawClipRegion) (),
	SWAP_FUNC_PREFIX(PEXSetViewPriority) (),
	SWAP_FUNC_PREFIX(PEXSetDisplayUpdateMode) (),
	SWAP_FUNC_PREFIX(PEXMapDCtoWC) (),
	SWAP_FUNC_PREFIX(PEXMapWCtoDC) (),
	SWAP_FUNC_PREFIX(PEXSetViewRep) (),
	SWAP_FUNC_PREFIX(PEXSetWksWindow) (),
	SWAP_FUNC_PREFIX(PEXSetWksViewport) (),
	SWAP_FUNC_PREFIX(PEXSetHlhsrMode) (),
	SWAP_FUNC_PREFIX(PEXSetWksBufferMode) (),
	SWAP_FUNC_PREFIX(PEXPostStructure) (),
	SWAP_FUNC_PREFIX(PEXUnpostStructure) (),
	SWAP_FUNC_PREFIX(PEXGetPickDevice) (),
	SWAP_FUNC_PREFIX(PEXChangePickDevice) (),
	SWAP_FUNC_PREFIX(PEXCreatePickMeasure) (),
	SWAP_FUNC_PREFIX(PEXGetPickMeasure) (),
	SWAP_FUNC_PREFIX(PEXUpdatePickMeasure) (),
	SWAP_FUNC_PREFIX(PEXOpenFont) (),
	SWAP_FUNC_PREFIX(PEXQueryFont) (),
	SWAP_FUNC_PREFIX(PEXListFonts) (),
	SWAP_FUNC_PREFIX(PEXListFontsWithInfo) (),
	SWAP_FUNC_PREFIX(PEXQueryTextExtents) (),
        SWAP_FUNC_PREFIX(PEXMatchRendererTargets) (),
	SWAP_FUNC_PREFIX(PEXEscape) (),
	SWAP_FUNC_PREFIX(PEXEscapeWithReply) (),
	SWAP_FUNC_PREFIX(PEXRenderElements) (),
	SWAP_FUNC_PREFIX(PEXAccumulateState) (),
	SWAP_FUNC_PREFIX(PEXBeginPickOne) (),
	SWAP_FUNC_PREFIX(PEXEndPickOne) (),
        SWAP_FUNC_PREFIX(PEXPickOne) (),
	SWAP_FUNC_PREFIX(PEXBeginPickAll) (),
	SWAP_FUNC_PREFIX(PEXEndPickAll) (),
	SWAP_FUNC_PREFIX(PEXPickAll) ();


LOCAL_FLAG void
	SWAP_FUNC_PREFIX(SwapTable)(),
	SWAP_FUNC_PREFIX(SwapSearchContext)(),
	SWAP_FUNC_PREFIX(SwapPickMeasAttr) (),
	SWAP_FUNC_PREFIX(SwapPickDevAttr) (),
	SWAP_FUNC_PREFIX(SwapPickRecord) (),
	SWAP_FUNC_PREFIX(SwapRendererAttributes) ();

LOCAL_FLAG unsigned char * SWAP_FUNC_PREFIX(SwapLightEntry) ();
LOCAL_FLAG unsigned char * SWAP_FUNC_PREFIX(SwapLineBundleEntry) ();
LOCAL_FLAG unsigned char * SWAP_FUNC_PREFIX(SwapMarkerBundleEntry) ();
LOCAL_FLAG unsigned char * SWAP_FUNC_PREFIX(SwapTextBundleEntry) ();
LOCAL_FLAG unsigned char * SWAP_FUNC_PREFIX(SwapInteriorBundleEntry) ();
LOCAL_FLAG unsigned char * SWAP_FUNC_PREFIX(SwapEdgeBundleEntry) ();
LOCAL_FLAG unsigned char * SWAP_FUNC_PREFIX(SwapDepthCueEntry) ();
LOCAL_FLAG unsigned char * SWAP_FUNC_PREFIX(SwapPatternEntry) ();
LOCAL_FLAG unsigned char * SWAP_FUNC_PREFIX(SwapPipelineContextAttr) ();

LOCAL_FLAG CARD8 * SWAP_FUNC_PREFIX(SwapFontInfo) ();


