/* $Xorg: Requests.h,v 1.4 2001/02/09 02:04:16 xorgcvs Exp $ */

/******************************************************************

Copyright 1990, 1991, 1998  The Open Group

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

Copyright 1990, 1991 by Sun Microsystems, Inc. 

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
#ifdef SWAP_FUNC_PREFIX
#undef SWAP_FUNC_PREFIX
#endif
#define SWAP_FUNC_PREFIX(nm) nm

LOCAL_FLAG ErrorCode
	SWAP_FUNC_PREFIX(PEXRequestUnused) (),
	SWAP_FUNC_PREFIX(PEXGetExtensionInfo) (),
	SWAP_FUNC_PREFIX(PEXGetEnumeratedTypeInfo) (),
	SWAP_FUNC_PREFIX(PEXGetImpDepConstants) (),
	SWAP_FUNC_PREFIX(PEXCreateLookupTable) (),
	SWAP_FUNC_PREFIX(PEXCopyLookupTable) (),
	SWAP_FUNC_PREFIX(PEXFreeLookupTable) (),
	SWAP_FUNC_PREFIX(PEXGetTableInfo) (),
	SWAP_FUNC_PREFIX(PEXGetPredefinedEntries) (),
	SWAP_FUNC_PREFIX(PEXGetDefinedIndices) (),
	SWAP_FUNC_PREFIX(PEXGetTableEntry) (),
	SWAP_FUNC_PREFIX(PEXGetTableEntries) (),
	SWAP_FUNC_PREFIX(PEXSetTableEntries) (),
	SWAP_FUNC_PREFIX(PEXDeleteTableEntries) (),
	SWAP_FUNC_PREFIX(PEXCreatePipelineContext) (),
	SWAP_FUNC_PREFIX(PEXCopyPipelineContext) (),
	SWAP_FUNC_PREFIX(PEXFreePipelineContext) (),
	SWAP_FUNC_PREFIX(PEXGetPipelineContext) (),
	SWAP_FUNC_PREFIX(PEXChangePipelineContext) (),
	SWAP_FUNC_PREFIX(PEXCreateRenderer) (),
	SWAP_FUNC_PREFIX(PEXFreeRenderer) (),
	SWAP_FUNC_PREFIX(PEXChangeRenderer) (),
	SWAP_FUNC_PREFIX(PEXGetRendererAttributes) (),
	SWAP_FUNC_PREFIX(PEXGetRendererDynamics) (),
	SWAP_FUNC_PREFIX(PEXBeginRendering) (),
	SWAP_FUNC_PREFIX(PEXEndRendering) (),
	SWAP_FUNC_PREFIX(PEXBeginStructure) (),
	SWAP_FUNC_PREFIX(PEXEndStructure) (),
	SWAP_FUNC_PREFIX(PEXRenderOutputCommands) (),
	SWAP_FUNC_PREFIX(PEXRenderNetwork) (),
	SWAP_FUNC_PREFIX(PEXCreateStructure) (),
	SWAP_FUNC_PREFIX(PEXCopyStructure) (),
	SWAP_FUNC_PREFIX(PEXDestroyStructures) (),
	SWAP_FUNC_PREFIX(PEXGetStructureInfo) (),
	SWAP_FUNC_PREFIX(PEXGetElementInfo) (),
	SWAP_FUNC_PREFIX(PEXGetStructuresInNetwork) (),
	SWAP_FUNC_PREFIX(PEXGetAncestors) (),
	SWAP_FUNC_PREFIX(PEXGetDescendants) (),
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
	SWAP_FUNC_PREFIX(PEXCreateNameSet) (),
	SWAP_FUNC_PREFIX(PEXCopyNameSet) (),
	SWAP_FUNC_PREFIX(PEXFreeNameSet) (),
	SWAP_FUNC_PREFIX(PEXGetNameSet) (),
	SWAP_FUNC_PREFIX(PEXChangeNameSet) (),
	SWAP_FUNC_PREFIX(PEXCreateSearchContext) (),
	SWAP_FUNC_PREFIX(PEXCopySearchContext) (),
	SWAP_FUNC_PREFIX(PEXFreeSearchContext) (),
	SWAP_FUNC_PREFIX(PEXGetSearchContext) (),
	SWAP_FUNC_PREFIX(PEXChangeSearchContext) (),
	SWAP_FUNC_PREFIX(PEXSearchNetwork) (),
	SWAP_FUNC_PREFIX(PEXCreatePhigsWks) (),
	SWAP_FUNC_PREFIX(PEXFreePhigsWks) (),
	SWAP_FUNC_PREFIX(PEXGetWksInfo) (),
	SWAP_FUNC_PREFIX(PEXGetDynamics) (),
	SWAP_FUNC_PREFIX(PEXGetViewRep) (),
	SWAP_FUNC_PREFIX(PEXRedrawAllStructures) (),
	SWAP_FUNC_PREFIX(PEXUpdateWorkstation) (),
	SWAP_FUNC_PREFIX(PEXRedrawClipRegion) (),
	SWAP_FUNC_PREFIX(PEXExecuteDeferredActions) (),
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
	SWAP_FUNC_PREFIX(PEXUnpostAllStructures) (),
	SWAP_FUNC_PREFIX(PEXGetWksPostings) (),
	SWAP_FUNC_PREFIX(PEXGetPickDevice) (),
	SWAP_FUNC_PREFIX(PEXChangePickDevice) (),
	SWAP_FUNC_PREFIX(PEXCreatePickMeasure) (),
	SWAP_FUNC_PREFIX(PEXFreePickMeasure) (),
	SWAP_FUNC_PREFIX(PEXGetPickMeasure) (),
	SWAP_FUNC_PREFIX(PEXUpdatePickMeasure) (),
	SWAP_FUNC_PREFIX(PEXOpenFont) (),
	SWAP_FUNC_PREFIX(PEXCloseFont) (),
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

RequestFunction SWAP_FUNC_PREFIX(PEXRequest)[] = {
/*   0	*/  SWAP_FUNC_PREFIX(PEXRequestUnused),
/*   1	*/  SWAP_FUNC_PREFIX(PEXGetExtensionInfo),
/*   2	*/  SWAP_FUNC_PREFIX(PEXGetEnumeratedTypeInfo),
/*   3	*/  SWAP_FUNC_PREFIX(PEXGetImpDepConstants),
/*   4	*/  SWAP_FUNC_PREFIX(PEXCreateLookupTable),
/*   5	*/  SWAP_FUNC_PREFIX(PEXCopyLookupTable),
/*   6	*/  SWAP_FUNC_PREFIX(PEXFreeLookupTable),
/*   7	*/  SWAP_FUNC_PREFIX(PEXGetTableInfo),
/*   8	*/  SWAP_FUNC_PREFIX(PEXGetPredefinedEntries),
/*   9	*/  SWAP_FUNC_PREFIX(PEXGetDefinedIndices),
/*   10	*/  SWAP_FUNC_PREFIX(PEXGetTableEntry),
/*   11	*/  SWAP_FUNC_PREFIX(PEXGetTableEntries),
/*   12	*/  SWAP_FUNC_PREFIX(PEXSetTableEntries),
/*   13	*/  SWAP_FUNC_PREFIX(PEXDeleteTableEntries),
/*   14	*/  SWAP_FUNC_PREFIX(PEXCreatePipelineContext),
/*   15	*/  SWAP_FUNC_PREFIX(PEXCopyPipelineContext),
/*   16	*/  SWAP_FUNC_PREFIX(PEXFreePipelineContext),
/*   17	*/  SWAP_FUNC_PREFIX(PEXGetPipelineContext),
/*   18	*/  SWAP_FUNC_PREFIX(PEXChangePipelineContext),
/*   19	*/  SWAP_FUNC_PREFIX(PEXCreateRenderer),
/*   20	*/  SWAP_FUNC_PREFIX(PEXFreeRenderer),
/*   21	*/  SWAP_FUNC_PREFIX(PEXChangeRenderer),
/*   22	*/  SWAP_FUNC_PREFIX(PEXGetRendererAttributes),
/*   23	*/  SWAP_FUNC_PREFIX(PEXGetRendererDynamics),
/*   24	*/  SWAP_FUNC_PREFIX(PEXBeginRendering),
/*   25	*/  SWAP_FUNC_PREFIX(PEXEndRendering),
/*   26	*/  SWAP_FUNC_PREFIX(PEXBeginStructure),
/*   27	*/  SWAP_FUNC_PREFIX(PEXEndStructure),
/*   28	*/  SWAP_FUNC_PREFIX(PEXRenderOutputCommands),
/*   29	*/  SWAP_FUNC_PREFIX(PEXRenderNetwork),
/*   30	*/  SWAP_FUNC_PREFIX(PEXCreateStructure),
/*   31	*/  SWAP_FUNC_PREFIX(PEXCopyStructure),
/*   32	*/  SWAP_FUNC_PREFIX(PEXDestroyStructures),
/*   33	*/  SWAP_FUNC_PREFIX(PEXGetStructureInfo),
/*   34	*/  SWAP_FUNC_PREFIX(PEXGetElementInfo),
/*   35	*/  SWAP_FUNC_PREFIX(PEXGetStructuresInNetwork),
/*   36	*/  SWAP_FUNC_PREFIX(PEXGetAncestors),
/*   37	*/  SWAP_FUNC_PREFIX(PEXGetDescendants),
/*   38	*/  SWAP_FUNC_PREFIX(PEXFetchElements),
/*   39	*/  SWAP_FUNC_PREFIX(PEXSetEditingMode),
/*   40	*/  SWAP_FUNC_PREFIX(PEXSetElementPointer),
/*   41	*/  SWAP_FUNC_PREFIX(PEXSetElementPointerAtLabel),
/*   42	*/  SWAP_FUNC_PREFIX(PEXElementSearch),
/*   43	*/  SWAP_FUNC_PREFIX(PEXStoreElements),
/*   44	*/  SWAP_FUNC_PREFIX(PEXDeleteElements),
/*   45	*/  SWAP_FUNC_PREFIX(PEXDeleteElementsToLabel),
/*   46	*/  SWAP_FUNC_PREFIX(PEXDeleteBetweenLabels),
/*   47	*/  SWAP_FUNC_PREFIX(PEXCopyElements),
/*   48	*/  SWAP_FUNC_PREFIX(PEXChangeStructureRefs),
/*   49	*/  SWAP_FUNC_PREFIX(PEXCreateNameSet),
/*   50	*/  SWAP_FUNC_PREFIX(PEXCopyNameSet),
/*   51	*/  SWAP_FUNC_PREFIX(PEXFreeNameSet),
/*   52	*/  SWAP_FUNC_PREFIX(PEXGetNameSet),
/*   53	*/  SWAP_FUNC_PREFIX(PEXChangeNameSet),
/*   54	*/  SWAP_FUNC_PREFIX(PEXCreateSearchContext),
/*   55	*/  SWAP_FUNC_PREFIX(PEXCopySearchContext),
/*   56	*/  SWAP_FUNC_PREFIX(PEXFreeSearchContext),
/*   57	*/  SWAP_FUNC_PREFIX(PEXGetSearchContext),
/*   58	*/  SWAP_FUNC_PREFIX(PEXChangeSearchContext),
/*   59	*/  SWAP_FUNC_PREFIX(PEXSearchNetwork),
/*   60	*/  SWAP_FUNC_PREFIX(PEXCreatePhigsWks),
/*   61	*/  SWAP_FUNC_PREFIX(PEXFreePhigsWks),
/*   62	*/  SWAP_FUNC_PREFIX(PEXGetWksInfo),
/*   63	*/  SWAP_FUNC_PREFIX(PEXGetDynamics),
/*   64	*/  SWAP_FUNC_PREFIX(PEXGetViewRep),
/*   65	*/  SWAP_FUNC_PREFIX(PEXRedrawAllStructures),
/*   66	*/  SWAP_FUNC_PREFIX(PEXUpdateWorkstation),
/*   67	*/  SWAP_FUNC_PREFIX(PEXRedrawClipRegion),
/*   68	*/  SWAP_FUNC_PREFIX(PEXExecuteDeferredActions),
/*   69	*/  SWAP_FUNC_PREFIX(PEXSetViewPriority),
/*   70	*/  SWAP_FUNC_PREFIX(PEXSetDisplayUpdateMode),
/*   71	*/  SWAP_FUNC_PREFIX(PEXMapDCtoWC),
/*   72	*/  SWAP_FUNC_PREFIX(PEXMapWCtoDC),
/*   73	*/  SWAP_FUNC_PREFIX(PEXSetViewRep),
/*   74	*/  SWAP_FUNC_PREFIX(PEXSetWksWindow),
/*   75	*/  SWAP_FUNC_PREFIX(PEXSetWksViewport),
/*   76	*/  SWAP_FUNC_PREFIX(PEXSetHlhsrMode),
/*   77	*/  SWAP_FUNC_PREFIX(PEXSetWksBufferMode),
/*   78	*/  SWAP_FUNC_PREFIX(PEXPostStructure),
/*   79	*/  SWAP_FUNC_PREFIX(PEXUnpostStructure),
/*   80	*/  SWAP_FUNC_PREFIX(PEXUnpostAllStructures),
/*   81	*/  SWAP_FUNC_PREFIX(PEXGetWksPostings),
/*   82	*/  SWAP_FUNC_PREFIX(PEXGetPickDevice),
/*   83	*/  SWAP_FUNC_PREFIX(PEXChangePickDevice),
/*   84	*/  SWAP_FUNC_PREFIX(PEXCreatePickMeasure),
/*   85	*/  SWAP_FUNC_PREFIX(PEXFreePickMeasure),
/*   86	*/  SWAP_FUNC_PREFIX(PEXGetPickMeasure),
/*   87	*/  SWAP_FUNC_PREFIX(PEXUpdatePickMeasure),
/*   88	*/  SWAP_FUNC_PREFIX(PEXOpenFont),
/*   89	*/  SWAP_FUNC_PREFIX(PEXCloseFont),
/*   90	*/  SWAP_FUNC_PREFIX(PEXQueryFont),
/*   91	*/  SWAP_FUNC_PREFIX(PEXListFonts),
/*   92	*/  SWAP_FUNC_PREFIX(PEXListFontsWithInfo),
/*   93	*/  SWAP_FUNC_PREFIX(PEXQueryTextExtents),
/*   94	*/  SWAP_FUNC_PREFIX(PEXMatchRendererTargets),
/*   95	*/  SWAP_FUNC_PREFIX(PEXEscape),
/*   96	*/  SWAP_FUNC_PREFIX(PEXEscapeWithReply),
/*   97	*/  SWAP_FUNC_PREFIX(PEXRenderElements),
/*   98	*/  SWAP_FUNC_PREFIX(PEXAccumulateState),
/*   99	*/  SWAP_FUNC_PREFIX(PEXBeginPickOne),
/*   100	*/  SWAP_FUNC_PREFIX(PEXEndPickOne),
/*   101	*/  SWAP_FUNC_PREFIX(PEXPickOne),
/*   102	*/  SWAP_FUNC_PREFIX(PEXBeginPickAll),
/*   103	*/  SWAP_FUNC_PREFIX(PEXEndPickAll),
/*   104	*/  SWAP_FUNC_PREFIX(PEXPickAll)
};

#undef SWAP_FUNC_PREFIX

