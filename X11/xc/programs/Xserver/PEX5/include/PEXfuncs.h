/* $Xorg: PEXfuncs.h,v 1.4 2001/02/09 02:04:18 xorgcvs Exp $ */

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

#ifndef PEX_FUNCS_H
#define PEX_FUNCS_H

/* PEX Function Declarations, in alphabetic order. */

extern int PEXBeginRendering();			/* in file pex/pex_rend.c */
extern int PEXBeginStructure();			/* in file pex/pex_rend.c */
extern int PEXChangeNameSet();			/* in file pex/pex_ns.c */
extern int PEXChangePickDevice();		/* in file pex/pex_pick.c */
extern int PEXChangePipelineContext();		/* in file pex/pex_pipe.c */
extern int PEXChangeRenderer();			/* in file pex/pex_rend.c */
extern int PEXChangeSearchContext();		/* in file pex/pex_srch.c */
extern int PEXChangeStructureRefs();		/* in file pex/pex_stru.c */
extern int PEXCloseFont();			/* in file pex/pex_font.c */
extern int PEXCopyElements();			/* in file pex/pex_stru.c */
extern int PEXCopyLookupTable();		/* in file pex/pex_lut.c */
extern int PEXCopyNameSet();			/* in file pex/pex_ns.c */
extern int PEXCopyPipelineContext();		/* in file pex/pex_pipe.c */
extern int PEXCopySearchContext();		/* in file pex/pex_srch.c */
extern int PEXCopyStructure();			/* in file pex/pex_stru.c */
extern int PEXCreateLookupTable();		/* in file pex/pex_lut.c */
extern int PEXCreateNameSet();			/* in file pex/pex_ns.c */
extern int PEXCreatePhigsWks();			/* in file pex/pex_ws.c */
extern int PEXCreatePickMeasure();		/* in file pex/pex_pick.c */
extern int PEXCreatePipelineContext();		/* in file pex/pex_pipe.c */
extern int PEXCreateRenderer();			/* in file pex/pex_rend.c */
extern int PEXCreateSearchContext();		/* in file pex/pex_srch.c */
extern int PEXCreateStructure();		/* in file pex/pex_stru.c */
extern int PEXDeleteBetweenLabels();		/* in file pex/pex_stru.c */
extern int PEXDeleteElements();			/* in file pex/pex_stru.c */
extern int PEXDeleteElementsToLabel();		/* in file pex/pex_stru.c */
extern int PEXDeleteTableEntries();		/* in file pex/pex_lut.c */
extern int PEXDestroyLookupTable();		/* in file pex/pex_lut.c */
extern int PEXDestroyNameSet();			/* in file pex/pex_ns.c */
extern int PEXDestroyPhigsWks();		/* in file pex/pex_ws.c */
extern int PEXDestroyPickMeasure();		/* in file pex/pex_pick.c */
extern int PEXDestroyPipelineContext();		/* in file pex/pex_pipe.c */
extern int PEXDestroyRenderer();		/* in file pex/pex_rend.c */
extern int PEXDestroySearchContext();		/* in file pex/pex_srch.c */
extern int PEXDestroyStructures();		/* in file pex/pex_stru.c */
extern int PEXElementSearch();			/* in file pex/pex_stru.c */
extern int PEXEndRendering();			/* in file pex/pex_rend.c */
extern int PEXEndStructure();			/* in file pex/pex_rend.c */
extern int PEXExecuteDeferredActions();		/* in file pex/pex_ws.c */
extern int PEXFetchElements();			/* in file pex/pex_stru.c */
extern int PEXGetAncestors();			/* in file pex/pex_stru.c */
extern int PEXGetDefinedIndices();		/* in file pex/pex_lut.c */
extern int PEXGetDescendants();			/* in file pex/pex_stru.c */
extern int PEXGetDynamics();			/* in file pex/pex_ws.c */
extern int PEXGetElementInfo();			/* in file pex/pex_stru.c */
extern int PEXGetEnumeratedTypeInfo();		/* in file pex/pex_info.c */
extern int PEXGetExtensionInfo();		/* in file pex/pex_info.c */
extern int PEXGetImpDepConstants();		/* in file pex/pex_info.c */
extern int PEXGetNameSet();			/* in file pex/pex_ns.c */
extern int PEXGetPickDevice();			/* in file pex/pex_pick.c */
extern int PEXGetPickMeasure();			/* in file pex/pex_pick.c */
extern int PEXGetPipelineContext();		/* in file pex/pex_pipe.c */
extern int PEXGetPredefinedEntries();		/* in file pex/pex_lut.c */
extern int PEXGetRendererAttributes();		/* in file pex/pex_rend.c */
extern int PEXGetRendererDynamics();		/* in file pex/pex_rend.c */
extern int PEXGetSearchContext();		/* in file pex/pex_srch.c */
extern int PEXGetStructureInfo();		/* in file pex/pex_stru.c */
extern int PEXGetStructuresInNetwork();		/* in file pex/pex_stru.c */
extern int PEXGetTableEntries();		/* in file pex/pex_lut.c */
extern int PEXGetTableEntry();			/* in file pex/pex_lut.c */
extern int PEXGetTableInfo();			/* in file pex/pex_lut.c */
extern int PEXGetViewRep();			/* in file pex/pex_ws.c */
extern int PEXGetWksInfo();			/* in file pex/pex_ws.c */
extern int PEXGetWksPostings();			/* in file pex/pex_ws.c */
extern int PEXListFonts();			/* in file pex/pex_font.c */
extern int PEXListFontsWithInfo();		/* in file pex/pex_font.c */
extern int PEXMapDCtoWC();			/* in file pex/pex_ws.c */
extern int PEXMapWCtoDC();			/* in file pex/pex_ws.c */
extern int PEXOpenFont();			/* in file pex/pex_font.c */
extern int PEXPostStructure();			/* in file pex/pex_ws.c */
extern int PEXQueryFont();			/* in file pex/pex_font.c */
extern int PEXQueryTextExtents();		/* in file pex/pex_font.c */
extern int PEXRedrawAllStructures();		/* in file pex/pex_ws.c */
extern int PEXRenderNetwork();			/* in file pex/pex_rend.c */
extern int PEXRenderOutputCommands();		/* in file pex/pex_rend.c */
extern int PEXSearchNetwork();			/* in file pex/pex_srch.c */
extern int PEXSetDisplayUpdateMode();		/* in file pex/pex_ws.c */
extern int PEXSetEditingMode();			/* in file pex/pex_stru.c */
extern int PEXSetElementPointer();		/* in file pex/pex_stru.c */
extern int PEXSetElementPointerAtLabel();	/* in file pex/pex_stru.c */
extern int PEXSetHlhsrMode();			/* in file pex/pex_ws.c */
extern int PEXSetTableEntries();		/* in file pex/pex_lut.c */
extern int PEXSetViewPriority();		/* in file pex/pex_ws.c */
extern int PEXSetViewRep();			/* in file pex/pex_ws.c */
extern int PEXSetWksViewport();			/* in file pex/pex_ws.c */
extern int PEXSetWksWindow();			/* in file pex/pex_ws.c */
extern int PEXStoreElements();			/* in file pex/pex_stru.c */
extern int PEXUnpostAllStructures();		/* in file pex/pex_ws.c */
extern int PEXUnpostStructure();		/* in file pex/pex_ws.c */
extern int PEXUpdatePickMeasure();		/* in file pex/pex_pick.c */
extern int PEXUpdateWorkstation();		/* in file pex/pex_ws.c */

#endif /* PEX_FUNCS_H */
