/* $Xorg: css_tbls.c,v 1.4 2001/02/09 02:04:11 xorgcvs Exp $ */
/*

Copyright 1989, 1990, 1991, 1998  The Open Group

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


Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 
All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Sun Microsystems
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/
/* Automatically generated element handling tables */

#include "ddpex.h"
#include "miStruct.h"


/* declarations for procs which handle elements stored in PEX format
 * this includes elements stored in PEX format and any extra elements
 * which are not defined in the protocol
 */

extern ddpex4rtn createCSS_Plain();
extern ddpex4rtn destroyCSS_Plain();
extern ddpex4rtn copyCSS_Plain();
extern ddpex4rtn replaceCSS_Plain();
extern ddpex4rtn inquireCSS_Plain();

/* declarations for procs which handle execute structure elements */
extern ddpex4rtn createCSS_Exec_Struct();
extern ddpex4rtn destroyCSS_Exec_Struct();
extern ddpex4rtn copyCSS_Exec_Struct();
extern ddpex4rtn replaceCSS_Exec_Struct();
extern ddpex4rtn inquireCSS_Exec_Struct();

cssTableType    CreateCSSElementTable[] = {
	createCSS_Plain,	/* 0 Propietary */
	createCSS_Plain,	/* 1 MarkerType */
	createCSS_Plain,	/* 2 MarkerScale */
	createCSS_Plain,	/* 3 MarkerColourIndex */
	createCSS_Plain,	/* 4 MarkerColour */
	createCSS_Plain,	/* 5 MarkerBundleIndex */
	createCSS_Plain,	/* 6 TextFontIndex */
	createCSS_Plain,	/* 7 TextPrecision */
	createCSS_Plain,	/* 8 CharExpansion */
	createCSS_Plain,	/* 9 CharSpacing */
	createCSS_Plain,	/* 10 TextColourIndex */
	createCSS_Plain,	/* 11 TextColour */
	createCSS_Plain,	/* 12 CharHeight */
	createCSS_Plain,	/* 13 CharUpVector */
	createCSS_Plain,	/* 14 TextPath */
	createCSS_Plain,	/* 15 TextAlignment */
	createCSS_Plain,	/* 16 AtextHeight */
	createCSS_Plain,	/* 17 AtextUpVector */
	createCSS_Plain,	/* 18 AtextPath */
	createCSS_Plain,	/* 19 AtextAlignment */
	createCSS_Plain,	/* 20 AtextStyle */
	createCSS_Plain,	/* 21 TextBundleIndex */
	createCSS_Plain,	/* 22 LineType */
	createCSS_Plain,	/* 23 LineWidth */
	createCSS_Plain,	/* 24 LineColourIndex */
	createCSS_Plain,	/* 25 LineColour */
	createCSS_Plain,	/* 26 CurveApproximation */
	createCSS_Plain,	/* 27 PolylineInterp */
	createCSS_Plain,	/* 28 LineBundleIndex */
	createCSS_Plain,	/* 29 InteriorStyle */
	createCSS_Plain,	/* 30 InteriorStyleIndex */
	createCSS_Plain,	/* 31 SurfaceColourIndex */
	createCSS_Plain,	/* 32 SurfaceColour */
	createCSS_Plain,	/* 33 SurfaceReflAttr */
	createCSS_Plain,	/* 34 SurfaceReflModel */
	createCSS_Plain,	/* 35 SurfaceInterp */
	createCSS_Plain,	/* 36 BfInteriorStyle */
	createCSS_Plain,	/* 37 BfInteriorStyleIndex */
	createCSS_Plain,	/* 38 BfSurfaceColourIndex */
	createCSS_Plain,	/* 39 BfSurfaceColour */
	createCSS_Plain,	/* 40 BfSurfaceReflAttr */
	createCSS_Plain,	/* 41 BfSurfaceReflModel */
	createCSS_Plain,	/* 42 BfSurfaceInterp */
	createCSS_Plain,	/* 43 SurfaceApproximation */
	createCSS_Plain,	/* 44 CullingMode */
	createCSS_Plain,	/* 45 DistinguishFlag */
	createCSS_Plain,	/* 46 PatternSize */
	createCSS_Plain,	/* 47 PatternRefPt */
	createCSS_Plain,	/* 48 PatternAttr */
	createCSS_Plain,	/* 49 InteriorBundleIndex */
	createCSS_Plain,	/* 50 SurfaceEdgeFlag */
	createCSS_Plain,	/* 51 SurfaceEdgeType */
	createCSS_Plain,	/* 52 SurfaceEdgeWidth */
	createCSS_Plain,	/* 53 SurfaceEdgeColourIndex */
	createCSS_Plain,	/* 54 SurfaceEdgeColour */
	createCSS_Plain,	/* 55 EdgeBundleIndex */
	createCSS_Plain,	/* 56 SetAsfValues */
	createCSS_Plain,	/* 57 LocalTransform */
	createCSS_Plain,	/* 58 LocalTransform2D */
	createCSS_Plain,	/* 59 GlobalTransform */
	createCSS_Plain,	/* 60 GlobalTransform2D */
	createCSS_Plain,	/* 61 ModelClip */
	createCSS_Plain,	/* 62 ModelClipVolume */
	createCSS_Plain,	/* 63 ModelClipVolume2D */
	createCSS_Plain,	/* 64 RestoreModelClip */
	createCSS_Plain,	/* 65 ViewIndex */
	createCSS_Plain,	/* 66 LightState */
	createCSS_Plain,	/* 67 DepthCueIndex */
	createCSS_Plain,	/* 68 PickId */
	createCSS_Plain,	/* 69 HlhsrIdentifier */
	createCSS_Plain,	/* 70 ColourApproxIndex */
	createCSS_Plain,	/* 71 RenderingColourModel */
	createCSS_Plain,	/* 72 PSurfaceCharacteristics */
	createCSS_Plain,	/* 73 AddToNameSet */
	createCSS_Plain,	/* 74 RemoveFromNameSet */
	createCSS_Exec_Struct,	/* 75 ExecuteStructure */
	createCSS_Plain,	/* 76 Label */
	createCSS_Plain,	/* 77 ApplicationData */
	createCSS_Plain,	/* 78 Gse */
	createCSS_Plain,	/* 79 Marker */
	createCSS_Plain,	/* 80 Marker2D */
	createCSS_Plain,	/* 81 Text */
	createCSS_Plain,	/* 82 Text2D */
	createCSS_Plain,	/* 83 AnnotationText */
	createCSS_Plain,	/* 84 AnnotationText2D */
	createCSS_Plain,	/* 85 Polyline */
	createCSS_Plain,	/* 86 Polyline2D */
	createCSS_Plain,	/* 87 PolylineSet */
	createCSS_Plain,	/* 88 NurbCurve */
	createCSS_Plain,	/* 89 FillArea */
	createCSS_Plain,	/* 90 FillArea2D */
	createCSS_Plain,	/* 91 ExtFillArea */
	createCSS_Plain,	/* 92 FillAreaSet */
	createCSS_Plain,	/* 93 FillAreaSet2D */
	createCSS_Plain,	/* 94 ExtFillAreaSet */
	createCSS_Plain,	/* 95 TriangleStrip */
	createCSS_Plain,	/* 96 QuadrilateralMesh */
	createCSS_Plain,	/* 97 SOFAS */
	createCSS_Plain,	/* 98 NurbSurface */
	createCSS_Plain,	/* 99 CellArray */
	createCSS_Plain,	/* 100 CellArray2D */
	createCSS_Plain,	/* 101 ExtCellArray */
	createCSS_Plain,	/* 102 Gdp */
	createCSS_Plain,	/* 103 Gdp2D */
	createCSS_Plain		/* 104 Noop */
};

cssTableType    DestroyCSSElementTable[] = {
	destroyCSS_Plain,	/* 0 Propietary */
	destroyCSS_Plain,	/* 1 MarkerType */
	destroyCSS_Plain,	/* 2 MarkerScale */
	destroyCSS_Plain,	/* 3 MarkerColourIndex */
	destroyCSS_Plain,	/* 4 MarkerColour */
	destroyCSS_Plain,	/* 5 MarkerBundleIndex */
	destroyCSS_Plain,	/* 6 TextFontIndex */
	destroyCSS_Plain,	/* 7 TextPrecision */
	destroyCSS_Plain,	/* 8 CharExpansion */
	destroyCSS_Plain,	/* 9 CharSpacing */
	destroyCSS_Plain,	/* 10 TextColourIndex */
	destroyCSS_Plain,	/* 11 TextColour */
	destroyCSS_Plain,	/* 12 CharHeight */
	destroyCSS_Plain,	/* 13 CharUpVector */
	destroyCSS_Plain,	/* 14 TextPath */
	destroyCSS_Plain,	/* 15 TextAlignment */
	destroyCSS_Plain,	/* 16 AtextHeight */
	destroyCSS_Plain,	/* 17 AtextUpVector */
	destroyCSS_Plain,	/* 18 AtextPath */
	destroyCSS_Plain,	/* 19 AtextAlignment */
	destroyCSS_Plain,	/* 20 AtextStyle */
	destroyCSS_Plain,	/* 21 TextBundleIndex */
	destroyCSS_Plain,	/* 22 LineType */
	destroyCSS_Plain,	/* 23 LineWidth */
	destroyCSS_Plain,	/* 24 LineColourIndex */
	destroyCSS_Plain,	/* 25 LineColour */
	destroyCSS_Plain,	/* 26 CurveApproximation */
	destroyCSS_Plain,	/* 27 PolylineInterp */
	destroyCSS_Plain,	/* 28 LineBundleIndex */
	destroyCSS_Plain,	/* 29 InteriorStyle */
	destroyCSS_Plain,	/* 30 InteriorStyleIndex */
	destroyCSS_Plain,	/* 31 SurfaceColourIndex */
	destroyCSS_Plain,	/* 32 SurfaceColour */
	destroyCSS_Plain,	/* 33 SurfaceReflAttr */
	destroyCSS_Plain,	/* 34 SurfaceReflModel */
	destroyCSS_Plain,	/* 35 SurfaceInterp */
	destroyCSS_Plain,	/* 36 BfInteriorStyle */
	destroyCSS_Plain,	/* 37 BfInteriorStyleIndex */
	destroyCSS_Plain,	/* 38 BfSurfaceColourIndex */
	destroyCSS_Plain,	/* 39 BfSurfaceColour */
	destroyCSS_Plain,	/* 40 BfSurfaceReflAttr */
	destroyCSS_Plain,	/* 41 BfSurfaceReflModel */
	destroyCSS_Plain,	/* 42 BfSurfaceInterp */
	destroyCSS_Plain,	/* 43 SurfaceApproximation */
	destroyCSS_Plain,	/* 44 CullingMode */
	destroyCSS_Plain,	/* 45 DistinguishFlag */
	destroyCSS_Plain,	/* 46 PatternSize */
	destroyCSS_Plain,	/* 47 PatternRefPt */
	destroyCSS_Plain,	/* 48 PatternAttr */
	destroyCSS_Plain,	/* 49 InteriorBundleIndex */
	destroyCSS_Plain,	/* 50 SurfaceEdgeFlag */
	destroyCSS_Plain,	/* 51 SurfaceEdgeType */
	destroyCSS_Plain,	/* 52 SurfaceEdgeWidth */
	destroyCSS_Plain,	/* 53 SurfaceEdgeColourIndex */
	destroyCSS_Plain,	/* 54 SurfaceEdgeColour */
	destroyCSS_Plain,	/* 55 EdgeBundleIndex */
	destroyCSS_Plain,	/* 56 SetAsfValues */
	destroyCSS_Plain,	/* 57 LocalTransform */
	destroyCSS_Plain,	/* 58 LocalTransform2D */
	destroyCSS_Plain,	/* 59 GlobalTransform */
	destroyCSS_Plain,	/* 60 GlobalTransform2D */
	destroyCSS_Plain,	/* 61 ModelClip */
	destroyCSS_Plain,	/* 62 ModelClipVolume */
	destroyCSS_Plain,	/* 63 ModelClipVolume2D */
	destroyCSS_Plain,	/* 64 RestoreModelClip */
	destroyCSS_Plain,	/* 65 ViewIndex */
	destroyCSS_Plain,	/* 66 LightState */
	destroyCSS_Plain,	/* 67 DepthCueIndex */
	destroyCSS_Plain,	/* 68 PickId */
	destroyCSS_Plain,	/* 69 HlhsrIdentifier */
	destroyCSS_Plain,	/* 70 ColourApproxIndex */
	destroyCSS_Plain,	/* 71 RenderingColourModel */
	destroyCSS_Plain,	/* 72 PSurfaceCharacteristics */
	destroyCSS_Plain,	/* 73 AddToNameSet */
	destroyCSS_Plain,	/* 74 RemoveFromNameSet */
	destroyCSS_Exec_Struct,	/* 75 ExecuteStructure */
	destroyCSS_Plain,	/* 76 Label */
	destroyCSS_Plain,	/* 77 ApplicationData */
	destroyCSS_Plain,	/* 78 Gse */
	destroyCSS_Plain,	/* 79 Marker */
	destroyCSS_Plain,	/* 80 Marker2D */
	destroyCSS_Plain,	/* 81 Text */
	destroyCSS_Plain,	/* 82 Text2D */
	destroyCSS_Plain,	/* 83 AnnotationText */
	destroyCSS_Plain,	/* 84 AnnotationText2D */
	destroyCSS_Plain,	/* 85 Polyline */
	destroyCSS_Plain,	/* 86 Polyline2D */
	destroyCSS_Plain,	/* 87 PolylineSet */
	destroyCSS_Plain,	/* 88 NurbCurve */
	destroyCSS_Plain,	/* 89 FillArea */
	destroyCSS_Plain,	/* 90 FillArea2D */
	destroyCSS_Plain,	/* 91 ExtFillArea */
	destroyCSS_Plain,	/* 92 FillAreaSet */
	destroyCSS_Plain,	/* 93 FillAreaSet2D */
	destroyCSS_Plain,	/* 94 ExtFillAreaSet */
	destroyCSS_Plain,	/* 95 TriangleStrip */
	destroyCSS_Plain,	/* 96 QuadrilateralMesh */
	destroyCSS_Plain,	/* 97 SOFAS */
	destroyCSS_Plain,	/* 98 NurbSurface */
	destroyCSS_Plain,	/* 99 CellArray */
	destroyCSS_Plain,	/* 100 CellArray2D */
	destroyCSS_Plain,	/* 101 ExtCellArray */
	destroyCSS_Plain,	/* 102 Gdp */
	destroyCSS_Plain,	/* 103 Gdp2D */
	destroyCSS_Plain	/* 104 Noop */
};

cssTableType    CopyCSSElementTable[] = {
	copyCSS_Plain,		/* 0 Propietary */
	copyCSS_Plain,		/* 1 MarkerType */
	copyCSS_Plain,		/* 2 MarkerScale */
	copyCSS_Plain,		/* 3 MarkerColourIndex */
	copyCSS_Plain,		/* 4 MarkerColour */
	copyCSS_Plain,		/* 5 MarkerBundleIndex */
	copyCSS_Plain,		/* 6 TextFontIndex */
	copyCSS_Plain,		/* 7 TextPrecision */
	copyCSS_Plain,		/* 8 CharExpansion */
	copyCSS_Plain,		/* 9 CharSpacing */
	copyCSS_Plain,		/* 10 TextColourIndex */
	copyCSS_Plain,		/* 11 TextColour */
	copyCSS_Plain,		/* 12 CharHeight */
	copyCSS_Plain,		/* 13 CharUpVector */
	copyCSS_Plain,		/* 14 TextPath */
	copyCSS_Plain,		/* 15 TextAlignment */
	copyCSS_Plain,		/* 16 AtextHeight */
	copyCSS_Plain,		/* 17 AtextUpVector */
	copyCSS_Plain,		/* 18 AtextPath */
	copyCSS_Plain,		/* 19 AtextAlignment */
	copyCSS_Plain,		/* 20 AtextStyle */
	copyCSS_Plain,		/* 21 TextBundleIndex */
	copyCSS_Plain,		/* 22 LineType */
	copyCSS_Plain,		/* 23 LineWidth */
	copyCSS_Plain,		/* 24 LineColourIndex */
	copyCSS_Plain,		/* 25 LineColour */
	copyCSS_Plain,		/* 26 CurveApproximation */
	copyCSS_Plain,		/* 27 PolylineInterp */
	copyCSS_Plain,		/* 28 LineBundleIndex */
	copyCSS_Plain,		/* 29 InteriorStyle */
	copyCSS_Plain,		/* 30 InteriorStyleIndex */
	copyCSS_Plain,		/* 31 SurfaceColourIndex */
	copyCSS_Plain,		/* 32 SurfaceColour */
	copyCSS_Plain,		/* 33 SurfaceReflAttr */
	copyCSS_Plain,		/* 34 SurfaceReflModel */
	copyCSS_Plain,		/* 35 SurfaceInterp */
	copyCSS_Plain,		/* 36 BfInteriorStyle */
	copyCSS_Plain,		/* 37 BfInteriorStyleIndex */
	copyCSS_Plain,		/* 38 BfSurfaceColourIndex */
	copyCSS_Plain,		/* 39 BfSurfaceColour */
	copyCSS_Plain,		/* 40 BfSurfaceReflAttr */
	copyCSS_Plain,		/* 41 BfSurfaceReflModel */
	copyCSS_Plain,		/* 42 BfSurfaceInterp */
	copyCSS_Plain,		/* 43 SurfaceApproximation */
	copyCSS_Plain,		/* 44 CullingMode */
	copyCSS_Plain,		/* 45 DistinguishFlag */
	copyCSS_Plain,		/* 46 PatternSize */
	copyCSS_Plain,		/* 47 PatternRefPt */
	copyCSS_Plain,		/* 48 PatternAttr */
	copyCSS_Plain,		/* 49 InteriorBundleIndex */
	copyCSS_Plain,		/* 50 SurfaceEdgeFlag */
	copyCSS_Plain,		/* 51 SurfaceEdgeType */
	copyCSS_Plain,		/* 52 SurfaceEdgeWidth */
	copyCSS_Plain,		/* 53 SurfaceEdgeColourIndex */
	copyCSS_Plain,		/* 54 SurfaceEdgeColour */
	copyCSS_Plain,		/* 55 EdgeBundleIndex */
	copyCSS_Plain,		/* 56 SetAsfValues */
	copyCSS_Plain,		/* 57 LocalTransform */
	copyCSS_Plain,		/* 58 LocalTransform2D */
	copyCSS_Plain,		/* 59 GlobalTransform */
	copyCSS_Plain,		/* 60 GlobalTransform2D */
	copyCSS_Plain,		/* 61 ModelClip */
	copyCSS_Plain,		/* 62 ModelClipVolume */
	copyCSS_Plain,		/* 63 ModelClipVolume2D */
	copyCSS_Plain,		/* 64 RestoreModelClip */
	copyCSS_Plain,		/* 65 ViewIndex */
	copyCSS_Plain,		/* 66 LightState */
	copyCSS_Plain,		/* 67 DepthCueIndex */
	copyCSS_Plain,		/* 68 PickId */
	copyCSS_Plain,		/* 69 HlhsrIdentifier */
	copyCSS_Plain,		/* 70 ColourApproxIndex */
	copyCSS_Plain,		/* 71 RenderingColourModel */
	copyCSS_Plain,		/* 72 PSurfaceCharacteristics */
	copyCSS_Plain,		/* 73 AddToNameSet */
	copyCSS_Plain,		/* 74 RemoveFromNameSet */
	copyCSS_Exec_Struct,	/* 75 ExecuteStructure */
	copyCSS_Plain,		/* 76 Label */
	copyCSS_Plain,		/* 77 ApplicationData */
	copyCSS_Plain,		/* 78 Gse */
	copyCSS_Plain,		/* 79 Marker */
	copyCSS_Plain,		/* 80 Marker2D */
	copyCSS_Plain,		/* 81 Text */
	copyCSS_Plain,		/* 82 Text2D */
	copyCSS_Plain,		/* 83 AnnotationText */
	copyCSS_Plain,		/* 84 AnnotationText2D */
	copyCSS_Plain,		/* 85 Polyline */
	copyCSS_Plain,		/* 86 Polyline2D */
	copyCSS_Plain,		/* 87 PolylineSet */
	copyCSS_Plain,		/* 88 NurbCurve */
	copyCSS_Plain,		/* 89 FillArea */
	copyCSS_Plain,		/* 90 FillArea2D */
	copyCSS_Plain,		/* 91 ExtFillArea */
	copyCSS_Plain,		/* 92 FillAreaSet */
	copyCSS_Plain,		/* 93 FillAreaSet2D */
	copyCSS_Plain,		/* 94 ExtFillAreaSet */
	copyCSS_Plain,		/* 95 TriangleStrip */
	copyCSS_Plain,		/* 96 QuadrilateralMesh */
	copyCSS_Plain,		/* 97 SOFAS */
	copyCSS_Plain,		/* 98 NurbSurface */
	copyCSS_Plain,		/* 99 CellArray */
	copyCSS_Plain,		/* 100 CellArray2D */
	copyCSS_Plain,		/* 101 ExtCellArray */
	copyCSS_Plain,		/* 102 Gdp */
	copyCSS_Plain,		/* 103 Gdp2D */
	copyCSS_Plain		/* 104 Noop */
};

cssTableType    ReplaceCSSElementTable[] = {
	replaceCSS_Plain,	/* 0 Propietary */
	replaceCSS_Plain,	/* 1 MarkerType */
	replaceCSS_Plain,	/* 2 MarkerScale */
	replaceCSS_Plain,	/* 3 MarkerColourIndex */
	replaceCSS_Plain,	/* 4 MarkerColour */
	replaceCSS_Plain,	/* 5 MarkerBundleIndex */
	replaceCSS_Plain,	/* 6 TextFontIndex */
	replaceCSS_Plain,	/* 7 TextPrecision */
	replaceCSS_Plain,	/* 8 CharExpansion */
	replaceCSS_Plain,	/* 9 CharSpacing */
	replaceCSS_Plain,	/* 10 TextColourIndex */
	replaceCSS_Plain,	/* 11 TextColour */
	replaceCSS_Plain,	/* 12 CharHeight */
	replaceCSS_Plain,	/* 13 CharUpVector */
	replaceCSS_Plain,	/* 14 TextPath */
	replaceCSS_Plain,	/* 15 TextAlignment */
	replaceCSS_Plain,	/* 16 AtextHeight */
	replaceCSS_Plain,	/* 17 AtextUpVector */
	replaceCSS_Plain,	/* 18 AtextPath */
	replaceCSS_Plain,	/* 19 AtextAlignment */
	replaceCSS_Plain,	/* 20 AtextStyle */
	replaceCSS_Plain,	/* 21 TextBundleIndex */
	replaceCSS_Plain,	/* 22 LineType */
	replaceCSS_Plain,	/* 23 LineWidth */
	replaceCSS_Plain,	/* 24 LineColourIndex */
	replaceCSS_Plain,	/* 25 LineColour */
	replaceCSS_Plain,	/* 26 CurveApproximation */
	replaceCSS_Plain,	/* 27 PolylineInterp */
	replaceCSS_Plain,	/* 28 LineBundleIndex */
	replaceCSS_Plain,	/* 29 InteriorStyle */
	replaceCSS_Plain,	/* 30 InteriorStyleIndex */
	replaceCSS_Plain,	/* 31 SurfaceColourIndex */
	replaceCSS_Plain,	/* 32 SurfaceColour */
	replaceCSS_Plain,	/* 33 SurfaceReflAttr */
	replaceCSS_Plain,	/* 34 SurfaceReflModel */
	replaceCSS_Plain,	/* 35 SurfaceInterp */
	replaceCSS_Plain,	/* 36 BfInteriorStyle */
	replaceCSS_Plain,	/* 37 BfInteriorStyleIndex */
	replaceCSS_Plain,	/* 38 BfSurfaceColourIndex */
	replaceCSS_Plain,	/* 39 BfSurfaceColour */
	replaceCSS_Plain,	/* 40 BfSurfaceReflAttr */
	replaceCSS_Plain,	/* 41 BfSurfaceReflModel */
	replaceCSS_Plain,	/* 42 BfSurfaceInterp */
	replaceCSS_Plain,	/* 43 SurfaceApproximation */
	replaceCSS_Plain,	/* 44 CullingMode */
	replaceCSS_Plain,	/* 45 DistinguishFlag */
	replaceCSS_Plain,	/* 46 PatternSize */
	replaceCSS_Plain,	/* 47 PatternRefPt */
	replaceCSS_Plain,	/* 48 PatternAttr */
	replaceCSS_Plain,	/* 49 InteriorBundleIndex */
	replaceCSS_Plain,	/* 50 SurfaceEdgeFlag */
	replaceCSS_Plain,	/* 51 SurfaceEdgeType */
	replaceCSS_Plain,	/* 52 SurfaceEdgeWidth */
	replaceCSS_Plain,	/* 53 SurfaceEdgeColourIndex */
	replaceCSS_Plain,	/* 54 SurfaceEdgeColour */
	replaceCSS_Plain,	/* 55 EdgeBundleIndex */
	replaceCSS_Plain,	/* 56 SetAsfValues */
	replaceCSS_Plain,	/* 57 LocalTransform */
	replaceCSS_Plain,	/* 58 LocalTransform2D */
	replaceCSS_Plain,	/* 59 GlobalTransform */
	replaceCSS_Plain,	/* 60 GlobalTransform2D */
	replaceCSS_Plain,	/* 61 ModelClip */
	replaceCSS_Plain,	/* 62 ModelClipVolume */
	replaceCSS_Plain,	/* 63 ModelClipVolume2D */
	replaceCSS_Plain,	/* 64 RestoreModelClip */
	replaceCSS_Plain,	/* 65 ViewIndex */
	replaceCSS_Plain,	/* 66 LightState */
	replaceCSS_Plain,	/* 67 DepthCueIndex */
	replaceCSS_Plain,	/* 68 PickId */
	replaceCSS_Plain,	/* 69 HlhsrIdentifier */
	replaceCSS_Plain,	/* 70 ColourApproxIndex */
	replaceCSS_Plain,	/* 71 RenderingColourModel */
	replaceCSS_Plain,	/* 72 PSurfaceCharacteristics */
	replaceCSS_Plain,	/* 73 AddToNameSet */
	replaceCSS_Plain,	/* 74 RemoveFromNameSet */
	replaceCSS_Exec_Struct,	/* 75 ExecuteStructure */
	replaceCSS_Plain,	/* 76 Label */
	replaceCSS_Plain,	/* 77 ApplicationData */
	replaceCSS_Plain,	/* 78 Gse */
	replaceCSS_Plain,	/* 79 Marker */
	replaceCSS_Plain,	/* 80 Marker2D */
	replaceCSS_Plain,	/* 81 Text */
	replaceCSS_Plain,	/* 82 Text2D */
	replaceCSS_Plain,	/* 83 AnnotationText */
	replaceCSS_Plain,	/* 84 AnnotationText2D */
	replaceCSS_Plain,	/* 85 Polyline */
	replaceCSS_Plain,	/* 86 Polyline2D */
	replaceCSS_Plain,	/* 87 PolylineSet */
	replaceCSS_Plain,	/* 88 NurbCurve */
	replaceCSS_Plain,	/* 89 FillArea */
	replaceCSS_Plain,	/* 90 FillArea2D */
	replaceCSS_Plain,	/* 91 ExtFillArea */
	replaceCSS_Plain,	/* 92 FillAreaSet */
	replaceCSS_Plain,	/* 93 FillAreaSet2D */
	replaceCSS_Plain,	/* 94 ExtFillAreaSet */
	replaceCSS_Plain,	/* 95 TriangleStrip */
	replaceCSS_Plain,	/* 96 QuadrilateralMesh */
	replaceCSS_Plain,	/* 97 SOFAS */
	replaceCSS_Plain,	/* 98 NurbSurface */
	replaceCSS_Plain,	/* 99 CellArray */
	replaceCSS_Plain,	/* 100 CellArray2D */
	replaceCSS_Plain,	/* 101 ExtCellArray */
	replaceCSS_Plain,	/* 102 Gdp */
	replaceCSS_Plain,	/* 103 Gdp2D */
	replaceCSS_Plain	/* 104 Noop */
};

cssTableType    InquireCSSElementTable[] = {
	inquireCSS_Plain,	/* 0 Propietary */
	inquireCSS_Plain,	/* 1 MarkerType */
	inquireCSS_Plain,	/* 2 MarkerScale */
	inquireCSS_Plain,	/* 3 MarkerColourIndex */
	inquireCSS_Plain,	/* 4 MarkerColour */
	inquireCSS_Plain,	/* 5 MarkerBundleIndex */
	inquireCSS_Plain,	/* 6 TextFontIndex */
	inquireCSS_Plain,	/* 7 TextPrecision */
	inquireCSS_Plain,	/* 8 CharExpansion */
	inquireCSS_Plain,	/* 9 CharSpacing */
	inquireCSS_Plain,	/* 10 TextColourIndex */
	inquireCSS_Plain,	/* 11 TextColour */
	inquireCSS_Plain,	/* 12 CharHeight */
	inquireCSS_Plain,	/* 13 CharUpVector */
	inquireCSS_Plain,	/* 14 TextPath */
	inquireCSS_Plain,	/* 15 TextAlignment */
	inquireCSS_Plain,	/* 16 AtextHeight */
	inquireCSS_Plain,	/* 17 AtextUpVector */
	inquireCSS_Plain,	/* 18 AtextPath */
	inquireCSS_Plain,	/* 19 AtextAlignment */
	inquireCSS_Plain,	/* 20 AtextStyle */
	inquireCSS_Plain,	/* 21 TextBundleIndex */
	inquireCSS_Plain,	/* 22 LineType */
	inquireCSS_Plain,	/* 23 LineWidth */
	inquireCSS_Plain,	/* 24 LineColourIndex */
	inquireCSS_Plain,	/* 25 LineColour */
	inquireCSS_Plain,	/* 26 CurveApproximation */
	inquireCSS_Plain,	/* 27 PolylineInterp */
	inquireCSS_Plain,	/* 28 LineBundleIndex */
	inquireCSS_Plain,	/* 29 InteriorStyle */
	inquireCSS_Plain,	/* 30 InteriorStyleIndex */
	inquireCSS_Plain,	/* 31 SurfaceColourIndex */
	inquireCSS_Plain,	/* 32 SurfaceColour */
	inquireCSS_Plain,	/* 33 SurfaceReflAttr */
	inquireCSS_Plain,	/* 34 SurfaceReflModel */
	inquireCSS_Plain,	/* 35 SurfaceInterp */
	inquireCSS_Plain,	/* 36 BfInteriorStyle */
	inquireCSS_Plain,	/* 37 BfInteriorStyleIndex */
	inquireCSS_Plain,	/* 38 BfSurfaceColourIndex */
	inquireCSS_Plain,	/* 39 BfSurfaceColour */
	inquireCSS_Plain,	/* 40 BfSurfaceReflAttr */
	inquireCSS_Plain,	/* 41 BfSurfaceReflModel */
	inquireCSS_Plain,	/* 42 BfSurfaceInterp */
	inquireCSS_Plain,	/* 43 SurfaceApproximation */
	inquireCSS_Plain,	/* 44 CullingMode */
	inquireCSS_Plain,	/* 45 DistinguishFlag */
	inquireCSS_Plain,	/* 46 PatternSize */
	inquireCSS_Plain,	/* 47 PatternRefPt */
	inquireCSS_Plain,	/* 48 PatternAttr */
	inquireCSS_Plain,	/* 49 InteriorBundleIndex */
	inquireCSS_Plain,	/* 50 SurfaceEdgeFlag */
	inquireCSS_Plain,	/* 51 SurfaceEdgeType */
	inquireCSS_Plain,	/* 52 SurfaceEdgeWidth */
	inquireCSS_Plain,	/* 53 SurfaceEdgeColourIndex */
	inquireCSS_Plain,	/* 54 SurfaceEdgeColour */
	inquireCSS_Plain,	/* 55 EdgeBundleIndex */
	inquireCSS_Plain,	/* 56 SetAsfValues */
	inquireCSS_Plain,	/* 57 LocalTransform */
	inquireCSS_Plain,	/* 58 LocalTransform2D */
	inquireCSS_Plain,	/* 59 GlobalTransform */
	inquireCSS_Plain,	/* 60 GlobalTransform2D */
	inquireCSS_Plain,	/* 61 ModelClip */
	inquireCSS_Plain,	/* 62 ModelClipVolume */
	inquireCSS_Plain,	/* 63 ModelClipVolume2D */
	inquireCSS_Plain,	/* 64 RestoreModelClip */
	inquireCSS_Plain,	/* 65 ViewIndex */
	inquireCSS_Plain,	/* 66 LightState */
	inquireCSS_Plain,	/* 67 DepthCueIndex */
	inquireCSS_Plain,	/* 68 PickId */
	inquireCSS_Plain,	/* 69 HlhsrIdentifier */
	inquireCSS_Plain,	/* 70 ColourApproxIndex */
	inquireCSS_Plain,	/* 71 RenderingColourModel */
	inquireCSS_Plain,	/* 72 PSurfaceCharacteristics */
	inquireCSS_Plain,	/* 73 AddToNameSet */
	inquireCSS_Plain,	/* 74 RemoveFromNameSet */
	inquireCSS_Exec_Struct,	/* 75 ExecuteStructure */
	inquireCSS_Plain,	/* 76 Label */
	inquireCSS_Plain,	/* 77 ApplicationData */
	inquireCSS_Plain,	/* 78 Gse */
	inquireCSS_Plain,	/* 79 Marker */
	inquireCSS_Plain,	/* 80 Marker2D */
	inquireCSS_Plain,	/* 81 Text */
	inquireCSS_Plain,	/* 82 Text2D */
	inquireCSS_Plain,	/* 83 AnnotationText */
	inquireCSS_Plain,	/* 84 AnnotationText2D */
	inquireCSS_Plain,	/* 85 Polyline */
	inquireCSS_Plain,	/* 86 Polyline2D */
	inquireCSS_Plain,	/* 87 PolylineSet */
	inquireCSS_Plain,	/* 88 NurbCurve */
	inquireCSS_Plain,	/* 89 FillArea */
	inquireCSS_Plain,	/* 90 FillArea2D */
	inquireCSS_Plain,	/* 91 ExtFillArea */
	inquireCSS_Plain,	/* 92 FillAreaSet */
	inquireCSS_Plain,	/* 93 FillAreaSet2D */
	inquireCSS_Plain,	/* 94 ExtFillAreaSet */
	inquireCSS_Plain,	/* 95 TriangleStrip */
	inquireCSS_Plain,	/* 96 QuadrilateralMesh */
	inquireCSS_Plain,	/* 97 SOFAS */
	inquireCSS_Plain,	/* 98 NurbSurface */
	inquireCSS_Plain,	/* 99 CellArray */
	inquireCSS_Plain,	/* 100 CellArray2D */
	inquireCSS_Plain,	/* 101 ExtCellArray */
	inquireCSS_Plain,	/* 102 Gdp */
	inquireCSS_Plain,	/* 103 Gdp2D */
	inquireCSS_Plain	/* 104 Noop */
};
