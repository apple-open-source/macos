## 
# $Xorg: OCReduce.sed,v 1.4 2001/02/09 02:04:17 xorgcvs Exp $
##
## Copyright 1996, 1998  The Open Group
##
## Permission to use, copy, modify, distribute, and sell this software and its
## documentation for any purpose is hereby granted without fee, provided that
## the above copyright notice appear in all copies and that both that
## copyright notice and this permission notice appear in supporting
## documentation.
##
## The above copyright notice and this permission notice shall be included in
## all copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
## OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
## AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
## CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
##
## Except as contained in this notice, the name of The Open Group shall not be
## used in advertising or otherwise to promote the sale, use or other dealings
## 
###########################################################################
## Copyright 1990, 1991 by Sun Microsystems, Inc.
## 
##                         All Rights Reserved
## 
## Permission to use, copy, modify, and distribute this software and its 
## documentation for any purpose and without fee is hereby granted, 
## provided that the above copyright notice appear in all copies and that
## both that copyright notice and this permission notice appear in 
## supporting documentation, and that the names of Sun Microsystems
## and The Open Group not be used in advertising or publicity 
## pertaining to distribution of the software without specific, written 
## prior permission.  
## 
## SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
## INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
## SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
## DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
## WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
## ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
## SOFTWARE.
## 
###########################################################################
## Usage:
##	rm -f temp.dat
##	echo "STUB_NAME " <stub_name> | cat > temp.dat
##	awk -f OCTables.awk temp.dat <pex-include-path>/PEX.h | awk -f OCReduce.awk | sed -f OCReduce.sed > <output_file>
##	
s/SWAP_OC_PREFIX(MarkerType)/SwapPEXMarkerType/g
s/SWAP_OC_PREFIX(MarkerScale)/SwapPEXMarkerScale/g
s/SWAP_OC_PREFIX(MarkerColourIndex)/SwapPEXMarkerColourIndex/g
s/SWAP_OC_PREFIX(MarkerBundleIndex)/SwapPEXMarkerBundleIndex/g
s/SWAP_OC_PREFIX(AtextStyle)/SwapPEXAtextStyle/g
s/SWAP_OC_PREFIX(TextPrecision)/SwapPEXTextPrecision/g
s/SWAP_OC_PREFIX(CharExpansion)/SwapPEXCharExpansion/g
s/SWAP_OC_PREFIX(CharSpacing)/SwapPEXCharSpacing/g
s/SWAP_OC_PREFIX(CharHeight)/SwapPEXCharHeight/g
s/SWAP_OC_PREFIX(CharUpVector)/SwapPEXCharUpVector/g
s/SWAP_OC_PREFIX(TextPath)/SwapPEXTextPath/g
s/SWAP_OC_PREFIX(TextAlignment)/SwapPEXTextAlignment/g
s/SWAP_OC_PREFIX(LineType)/SwapPEXLineType/g
s/SWAP_OC_PREFIX(LineWidth)/SwapPEXLineWidth/g
s/SWAP_OC_PREFIX(LineColourIndex)/SwapPEXLineColourIndex/g
s/SWAP_OC_PREFIX(CurveApproximation)/SwapPEXCurveApproximation/g
s/SWAP_OC_PREFIX(PolylineInterp)/SwapPEXPolylineInterp/g
s/SWAP_OC_PREFIX(InteriorStyle)/SwapPEXInteriorStyle/g
s/SWAP_OC_PREFIX(SurfaceColourIndex)/SwapPEXSurfaceColourIndex/g
s/SWAP_OC_PREFIX(SurfaceReflModel)/SwapPEXSurfaceReflModel/g
s/SWAP_OC_PREFIX(SurfaceInterp)/SwapPEXSurfaceInterp/g
s/SWAP_OC_PREFIX(SurfaceApproximation)/SwapPEXSurfaceApproximation/g
s/SWAP_OC_PREFIX(CullingMode)/SwapPEXCullingMode/g
s/SWAP_OC_PREFIX(DistinguishFlag)/SwapPEXDistinguishFlag/g
s/SWAP_OC_PREFIX(PatternSize)/SwapPEXPatternSize/g
s/SWAP_OC_PREFIX(PatternRefPt)/SwapPEXPatternRefPt/g
s/SWAP_OC_PREFIX(PatternAttr)/SwapPEXPatternAttr/g
s/SWAP_OC_PREFIX(SurfaceEdgeFlag)/SwapPEXSurfaceEdgeFlag/g
s/SWAP_OC_PREFIX(SurfaceEdgeType)/SwapPEXSurfaceEdgeType/g
s/SWAP_OC_PREFIX(SurfaceEdgeWidth)/SwapPEXSurfaceEdgeWidth/g
s/SWAP_OC_PREFIX(SetAsfValues)/SwapPEXSetAsfValues/g
s/SWAP_OC_PREFIX(LocalTransform)/SwapPEXLocalTransform/g
s/SWAP_OC_PREFIX(LocalTransform2D)/SwapPEXLocalTransform2D/g
s/SWAP_OC_PREFIX(GlobalTransform)/SwapPEXGlobalTransform/g
s/SWAP_OC_PREFIX(GlobalTransform2D)/SwapPEXGlobalTransform2D/g
s/SWAP_OC_PREFIX(ModelClip)/SwapPEXModelClip/g
s/SWAP_OC_PREFIX(RestoreModelClip)/SwapPEXRestoreModelClip/g
s/SWAP_OC_PREFIX(LightState)/SwapPEXLightState/g
s/SWAP_OC_PREFIX(PickId)/SwapPEXPickId/g
s/SWAP_OC_PREFIX(HlhsrIdentifier)/SwapPEXHlhsrIdentifier/g
s/SWAP_OC_PREFIX(ExecuteStructure)/SwapPEXExecuteStructure/g
s/SWAP_OC_PREFIX(Label)/SwapPEXLabel/g
s/SWAP_OC_PREFIX(ApplicationData)/SwapPEXApplicationData/g
s/SWAP_OC_PREFIX(Gse)/SwapPEXGse/g
s/SWAP_OC_PREFIX(RenderingColourModel)/SwapPEXRenderingColourModel/g
s/SWAP_OC_PREFIX(OCUnused)/SwapPEXOCUnused/g
