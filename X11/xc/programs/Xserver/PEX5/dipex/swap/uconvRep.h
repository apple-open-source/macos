/* $Xorg: uconvRep.h,v 1.4 2001/02/09 02:04:17 xorgcvs Exp $ */

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


LOCAL_FLAG void
    SWAP_FUNC_PREFIX(ConvertGetExtensionInfoReply) (),
    SWAP_FUNC_PREFIX(ConvertGetEnumeratedTypeInfoReply) (),
    SWAP_FUNC_PREFIX(ConvertGetImpDepConstantsReply) (),
    SWAP_FUNC_PREFIX(ConvertGetTableInfoReply) (),
    SWAP_FUNC_PREFIX(ConvertGetPredefinedEntriesReply) (),
    SWAP_FUNC_PREFIX(ConvertGetDefinedIndicesReply) (),
    SWAP_FUNC_PREFIX(ConvertGetTableEntryReply) (),
    SWAP_FUNC_PREFIX(ConvertGetTableEntriesReply) (),
    SWAP_FUNC_PREFIX(ConvertGetPipelineContextReply) (),
    SWAP_FUNC_PREFIX(ConvertGetRendererAttributesReply) (),
    SWAP_FUNC_PREFIX(ConvertGetRendererDynamicsReply) (),
    SWAP_FUNC_PREFIX(ConvertGetStructureInfoReply) (),
    SWAP_FUNC_PREFIX(ConvertGetElementInfoReply) (),
    SWAP_FUNC_PREFIX(ConvertGetStructuresInNetworkReply) (),
    SWAP_FUNC_PREFIX(ConvertGetAncestorsReply) (),
    SWAP_FUNC_PREFIX(ConvertElementSearchReply) (),
    SWAP_FUNC_PREFIX(ConvertFetchElementsReply) (),
    SWAP_FUNC_PREFIX(ConvertGetNameSetReply) (),
    SWAP_FUNC_PREFIX(ConvertGetSearchContextReply) (),
    SWAP_FUNC_PREFIX(ConvertSearchNetworkReply) (),
    SWAP_FUNC_PREFIX(ConvertGetWksInfoReply) (),
    SWAP_FUNC_PREFIX(ConvertGetDynamicsReply) (),
    SWAP_FUNC_PREFIX(ConvertGetViewRepReply) (),
    SWAP_FUNC_PREFIX(ConvertMapDCtoWCReply) (),
    SWAP_FUNC_PREFIX(ConvertMapWCtoDCReply) (),
    SWAP_FUNC_PREFIX(ConvertGetWksPostingsReply) (),
    SWAP_FUNC_PREFIX(ConvertGetPickDeviceReply) (),
    SWAP_FUNC_PREFIX(ConvertGetPickMeasureReply) (),
    SWAP_FUNC_PREFIX(ConvertQueryFontReply) (),
    SWAP_FUNC_PREFIX(ConvertListFontsReply) (),
    SWAP_FUNC_PREFIX(ConvertListFontsWithInfoReply) (),
    SWAP_FUNC_PREFIX(ConvertQueryTextExtentsReply) (),
    SWAP_FUNC_PREFIX(ConvertMatchRendererTargetsReply) (),
    SWAP_FUNC_PREFIX(EscapeWithReplyReply) (),
    SWAP_FUNC_PREFIX(ConvertEndPickOneReply) (),
    SWAP_FUNC_PREFIX(ConvertPickOneReply) (),
    SWAP_FUNC_PREFIX(ConvertEndPickAllReply) (),
    SWAP_FUNC_PREFIX(ConvertPickAllReply) (),
    SWAP_FUNC_PREFIX(NoReply)();


LOCAL_FLAG void
	SWAP_FUNC_PREFIX(SwapTable)(),
	SWAP_FUNC_PREFIX(SwapSearchContext)(),
	SWAP_FUNC_PREFIX(SwapPickMeasAttr) (),
	SWAP_FUNC_PREFIX(SwapPickDevAttr) (),
	SWAP_FUNC_PREFIX(SwapRendererAttributes) ();

LOCAL_FLAG unsigned char * SWAP_FUNC_PREFIX(SwapStringList) ();
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

