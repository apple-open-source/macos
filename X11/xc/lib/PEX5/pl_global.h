/* $Xorg: pl_global.h,v 1.4 2001/02/09 02:03:28 xorgcvs Exp $ */
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

/*
 * Header to linked list of open displays.
 */

PEXDisplayInfo 	*PEXDisplayInfoHeader = NULL;


/*
 * Pick path cache.
 */

PEXPickPath	*PEXPickCache = NULL;
unsigned int	PEXPickCacheSize = 0;
int		PEXPickCacheInUse = 0;


/*
 * Floating point conversion function table.
 */

extern void _PEXIEEE32toDECF();
extern void _PEXDECFtoIEEE32();

#ifdef CRAY
extern void _PEXIEEE32toCRAY();
extern void _PEXCRAYtoIEEE32();
#else
#define _PEXIEEE32toCRAY 0
#define _PEXCRAYtoIEEE32 0
#endif

void (*(PEX_fp_convert[NUM_FP_FORMATS][NUM_FP_FORMATS]))() = {

/* IEEE 32 -> OTHER */

    {0, _PEXIEEE32toDECF, 0, 0, _PEXIEEE32toCRAY},

/* DEC F -> OTHER */

    {_PEXDECFtoIEEE32, 0, 0, 0, 0},

/* IEEE 64 -> OTHER ------- NOT SUPPORTED !!! */

    {0, 0, 0, 0, 0},

/* DEC D -> OTHER   ------- NOT SUPPORTED !!! */

    {0, 0, 0, 0, 0},

/* CRAY -> OTHER */

    {_PEXCRAYtoIEEE32, 0, 0, 0, 0}

};


/*
 * Encode OC function table.
 */

extern void _PEXEncodeEnumType();
extern void _PEXEncodeTableIndex();
extern void _PEXEncodeColor();
extern void _PEXEncodeFloat();
extern void _PEXEncodeCARD16();
extern void _PEXEncodeVector2D();
extern void _PEXEncodeTextAlignment();
extern void _PEXEncodeCurveApprox();
extern void _PEXEncodeReflectionAttr();
extern void _PEXEncodeSurfaceApprox();
extern void _PEXEncodeCullMode();
extern void _PEXEncodeSwitch();
extern void _PEXEncodePatternSize();
extern void _PEXEncodePatternAttr2D();
extern void _PEXEncodePatternAttr();
extern void _PEXEncodeASF();
extern void _PEXEncodeLocalTransform();
extern void _PEXEncodeLocalTransform2D();
extern void _PEXEncodeGlobalTransform();
extern void _PEXEncodeGlobalTransform2D();
extern void _PEXEncodeModelClipVolume();
extern void _PEXEncodeModelClipVolume2D();
extern void _PEXEncodeRestoreModelClip();
extern void _PEXEncodeLightSourceState();
extern void _PEXEncodeID();
extern void _PEXEncodePSC();
extern void _PEXEncodeNameSet();
extern void _PEXEncodeExecuteStructure();
extern void _PEXEncodeLabel();
extern void _PEXEncodeApplicationData();
extern void _PEXEncodeGSE();
extern void _PEXEncodeMarkers();
extern void _PEXEncodePolyline();
extern void _PEXEncodeMarkers2D();
extern void _PEXEncodePolyline2D();
extern void _PEXEncodeText();
extern void _PEXEncodeText2D();
extern void _PEXEncodeAnnoText();
extern void _PEXEncodeAnnoText2D();
extern void _PEXEncodePolylineSet();
extern void _PEXEncodeNURBCurve();
extern void _PEXEncodeFillArea();
extern void _PEXEncodeFillArea2D();
extern void _PEXEncodeFillAreaWithData();
extern void _PEXEncodeFillAreaSet();
extern void _PEXEncodeFillAreaSet2D();
extern void _PEXEncodeFillAreaSetWithData();
extern void _PEXEncodeTriangleStrip();
extern void _PEXEncodeQuadMesh();
extern void _PEXEncodeSOFA();
extern void _PEXEncodeNURBSurface();
extern void _PEXEncodeCellArray();
extern void _PEXEncodeCellArray2D();
extern void _PEXEncodeExtendedCellArray();
extern void _PEXEncodeGDP();
extern void _PEXEncodeGDP2D();
extern void _PEXEncodeNoop();

void (*(PEX_encode_oc_funcs[]))() = {
NULL,				/* DUMMY		           0 */
_PEXEncodeEnumType,		/* PEXOCMarkerType                 1 */
_PEXEncodeFloat,		/* PEXOCMarkerScale                2 */
_PEXEncodeTableIndex,		/* PEXOCMarkerColorIndex           3 */
_PEXEncodeColor,		/* PEXOCMarkerColor                4 */
_PEXEncodeTableIndex,		/* PEXOCMarkerBundleIndex          5 */
_PEXEncodeTableIndex,		/* PEXOCTextFontIndex              6 */
_PEXEncodeCARD16,		/* PEXOCTextPrecision              7 */
_PEXEncodeFloat,		/* PEXOCCharExpansion              8 */
_PEXEncodeFloat,		/* PEXOCCharSpacing                9 */
_PEXEncodeTableIndex,		/* PEXOCTextColorIndex             10 */
_PEXEncodeColor,		/* PEXOCTextColor                  11 */
_PEXEncodeFloat,		/* PEXOCCharHeight                 12 */
_PEXEncodeVector2D,		/* PEXOCCharUpVector               13 */
_PEXEncodeCARD16,		/* PEXOCTextPath                   14 */
_PEXEncodeTextAlignment,	/* PEXOCTextAlignment              15 */
_PEXEncodeFloat,		/* PEXOCATextHeight                16 */
_PEXEncodeVector2D,		/* PEXOCATextUpVector              17 */
_PEXEncodeCARD16,		/* PEXOCATextPath                  18 */
_PEXEncodeTextAlignment,	/* PEXOCATextAlignment             19 */
_PEXEncodeEnumType,		/* PEXOCATextStyle                 20 */
_PEXEncodeTableIndex,		/* PEXOCTextBundleIndex            21 */
_PEXEncodeEnumType,		/* PEXOCLineType                   22 */
_PEXEncodeFloat,		/* PEXOCLineWidth                  23 */
_PEXEncodeTableIndex,		/* PEXOCLineColorIndex             24 */
_PEXEncodeColor,		/* PEXOCLineColor                  25 */
_PEXEncodeCurveApprox,		/* PEXOCCurveApprox                26 */
_PEXEncodeEnumType,		/* PEXOCPolylineInterpMethod       27 */
_PEXEncodeTableIndex,		/* PEXOCLineBundleIndex            28 */
_PEXEncodeEnumType,		/* PEXOCInteriorStyle              29 */
_PEXEncodeTableIndex,		/* PEXOCInteriorStyleIndex         30 */
_PEXEncodeTableIndex,		/* PEXOCSurfaceColorIndex          31 */
_PEXEncodeColor,		/* PEXOCSurfaceColor               32 */
_PEXEncodeReflectionAttr,	/* PEXOCReflectionAttributes       33 */
_PEXEncodeEnumType,		/* PEXOCReflectionModel            34 */
_PEXEncodeEnumType,		/* PEXOCSurfaceInterpMethod        35 */
_PEXEncodeEnumType,		/* PEXOCBFInteriorStyle            36 */
_PEXEncodeTableIndex,		/* PEXOCBFInteriorStyleIndex       37 */
_PEXEncodeTableIndex,		/* PEXOCBFSurfaceColorIndex        38 */
_PEXEncodeColor,		/* PEXOCBFSurfaceColor             39 */
_PEXEncodeReflectionAttr,	/* PEXOCBFReflectionAttributes     40 */
_PEXEncodeEnumType,		/* PEXOCBFReflectionModel          41 */
_PEXEncodeEnumType,		/* PEXOCBFSurfaceInterpMethod      42 */
_PEXEncodeSurfaceApprox,	/* PEXOCSurfaceApprox              43 */
_PEXEncodeCullMode,		/* PEXOCFacetCullingMode           44 */
_PEXEncodeSwitch,		/* PEXOCFacetDistinguishFlag       45 */
_PEXEncodePatternSize,		/* PEXOCPatternSize                46 */
_PEXEncodePatternAttr2D,	/* PEXOCPatternAttributes2D        47 */
_PEXEncodePatternAttr,		/* PEXOCPatternAttributes          48 */
_PEXEncodeTableIndex,		/* PEXOCInteriorBundleIndex        49 */
_PEXEncodeSwitch,		/* PEXOCSurfaceEdgeFlag            50 */
_PEXEncodeEnumType,		/* PEXOCSurfaceEdgeType            51 */
_PEXEncodeFloat,		/* PEXOCSurfaceEdgeWidth           52 */
_PEXEncodeTableIndex,		/* PEXOCSurfaceEdgeColorIndex      53 */
_PEXEncodeColor,		/* PEXOCSurfaceEdgeColor           54 */
_PEXEncodeTableIndex,		/* PEXOCEdgeBundleIndex            55 */
_PEXEncodeASF,			/* PEXOCIndividualASF              56 */
_PEXEncodeLocalTransform,	/* PEXOCLocalTransform             57 */
_PEXEncodeLocalTransform2D,	/* PEXOCLocalTransform2D           58 */
_PEXEncodeGlobalTransform,	/* PEXOCGlobalTransform            59 */
_PEXEncodeGlobalTransform2D,	/* PEXOCGlobalTransform2D          60 */
_PEXEncodeSwitch,		/* PEXOCModelClipFlag              61 */
_PEXEncodeModelClipVolume,	/* PEXOCModelClipVolume            62 */
_PEXEncodeModelClipVolume2D,	/* PEXOCModelClipVolume2D          63 */
_PEXEncodeRestoreModelClip,	/* PEXOCRestoreModelClipVolume     64 */
_PEXEncodeTableIndex,		/* PEXOCViewIndex                  65 */
_PEXEncodeLightSourceState,	/* PEXOCLightSourceState           66 */
_PEXEncodeTableIndex,		/* PEXOCDepthCueIndex              67 */
_PEXEncodeID,			/* PEXOCPickID                     68 */
_PEXEncodeID,			/* PEXOCHLHSRID                    69 */
_PEXEncodeTableIndex,		/* PEXOCColorApproxIndex           70 */
_PEXEncodeEnumType,		/* PEXOCRenderingColorModel        71 */
_PEXEncodePSC,			/* PEXOCParaSurfCharacteristics    72 */
_PEXEncodeNameSet,		/* PEXOCAddToNameSet               73 */
_PEXEncodeNameSet,		/* PEXOCRemoveFromNameSet          74 */
_PEXEncodeExecuteStructure,	/* PEXOCExecuteStructure           75 */
_PEXEncodeLabel,		/* PEXOCLabel                      76 */
_PEXEncodeApplicationData,	/* PEXOCApplicationData            77 */
_PEXEncodeGSE,			/* PEXOCGSE                        78 */
_PEXEncodeMarkers,		/* PEXOCMarkers                    79 */
_PEXEncodeMarkers2D,		/* PEXOCMarkers2D                  80 */
_PEXEncodeText,			/* PEXOCText                       81 */
_PEXEncodeText2D,		/* PEXOCText2D                     82 */
_PEXEncodeAnnoText,		/* PEXOCAnnotationText             83 */
_PEXEncodeAnnoText2D,		/* PEXOCAnnotationText2D           84 */
_PEXEncodePolyline,		/* PEXOCPolyline                   85 */
_PEXEncodePolyline2D,		/* PEXOCPolyline2D                 86 */
_PEXEncodePolylineSet,		/* PEXOCPolylineSetWithData        87 */
_PEXEncodeNURBCurve,		/* PEXOCNURBCurve                  88 */
_PEXEncodeFillArea,		/* PEXOCFillArea                   89 */
_PEXEncodeFillArea2D,		/* PEXOCFillArea2D                 90 */
_PEXEncodeFillAreaWithData,	/* PEXOCFillAreaWithData           91 */
_PEXEncodeFillAreaSet,		/* PEXOCFillAreaSet                92 */
_PEXEncodeFillAreaSet2D,	/* PEXOCFillAreaSet2D              93 */
_PEXEncodeFillAreaSetWithData,	/* PEXOCFillAreaSetWithData        94 */
_PEXEncodeTriangleStrip,	/* PEXOCTriangleStrip              95 */
_PEXEncodeQuadMesh,		/* PEXOCQuadrilateralMesh          96 */
_PEXEncodeSOFA,			/* PEXOCSetOfFillAreaSets          97 */
_PEXEncodeNURBSurface,		/* PEXOCNURBSurface                98 */
_PEXEncodeCellArray,		/* PEXOCCellArray                  99 */
_PEXEncodeCellArray2D,		/* PEXOCCellArray2D                100 */
_PEXEncodeExtendedCellArray,	/* PEXOCExtendedCellArray          101 */
_PEXEncodeGDP,			/* PEXOCGDP                        102 */
_PEXEncodeGDP2D,		/* PEXOCGDP2D                      103 */
_PEXEncodeNoop			/* PEXOCNoop                       104 */
};


/*
 * Decode OC function table.
 */

extern void _PEXDecodeEnumType();
extern void _PEXDecodeTableIndex();
extern void _PEXDecodeColor();
extern void _PEXDecodeFloat();
extern void _PEXDecodeCARD16();
extern void _PEXDecodeVector2D();
extern void _PEXDecodeTextAlignment();
extern void _PEXDecodeCurveApprox();
extern void _PEXDecodeReflectionAttr();
extern void _PEXDecodeSurfaceApprox();
extern void _PEXDecodeCullMode();
extern void _PEXDecodeSwitch();
extern void _PEXDecodePatternSize();
extern void _PEXDecodePatternAttr2D();
extern void _PEXDecodePatternAttr();
extern void _PEXDecodeASF();
extern void _PEXDecodeLocalTransform();
extern void _PEXDecodeLocalTransform2D();
extern void _PEXDecodeGlobalTransform();
extern void _PEXDecodeGlobalTransform2D();
extern void _PEXDecodeModelClipVolume();
extern void _PEXDecodeModelClipVolume2D();
extern void _PEXDecodeRestoreModelClip();
extern void _PEXDecodeLightSourceState();
extern void _PEXDecodeID();
extern void _PEXDecodePSC();
extern void _PEXDecodeNameSet();
extern void _PEXDecodeExecuteStructure();
extern void _PEXDecodeLabel();
extern void _PEXDecodeApplicationData();
extern void _PEXDecodeGSE();
extern void _PEXDecodeMarkers();
extern void _PEXDecodePolyline();
extern void _PEXDecodeMarkers2D();
extern void _PEXDecodePolyline2D();
extern void _PEXDecodeText();
extern void _PEXDecodeText2D();
extern void _PEXDecodeAnnoText();
extern void _PEXDecodeAnnoText2D();
extern void _PEXDecodePolylineSet();
extern void _PEXDecodeNURBCurve();
extern void _PEXDecodeFillArea();
extern void _PEXDecodeFillArea2D();
extern void _PEXDecodeFillAreaWithData();
extern void _PEXDecodeFillAreaSet();
extern void _PEXDecodeFillAreaSet2D();
extern void _PEXDecodeFillAreaSetWithData();
extern void _PEXDecodeTriangleStrip();
extern void _PEXDecodeQuadMesh();
extern void _PEXDecodeSOFA();
extern void _PEXDecodeNURBSurface();
extern void _PEXDecodeCellArray();
extern void _PEXDecodeCellArray2D();
extern void _PEXDecodeExtendedCellArray();
extern void _PEXDecodeGDP();
extern void _PEXDecodeGDP2D();
extern void _PEXDecodeNoop();

void (*(PEX_decode_oc_funcs[]))() = {
NULL,				/* DUMMY		           0 */
_PEXDecodeEnumType,		/* PEXOCMarkerType                 1 */
_PEXDecodeFloat,		/* PEXOCMarkerScale                2 */
_PEXDecodeTableIndex,		/* PEXOCMarkerColorIndex           3 */
_PEXDecodeColor,		/* PEXOCMarkerColor                4 */
_PEXDecodeTableIndex,		/* PEXOCMarkerBundleIndex          5 */
_PEXDecodeTableIndex,		/* PEXOCTextFontIndex              6 */
_PEXDecodeCARD16,		/* PEXOCTextPrecision              7 */
_PEXDecodeFloat,		/* PEXOCCharExpansion              8 */
_PEXDecodeFloat,		/* PEXOCCharSpacing                9 */
_PEXDecodeTableIndex,		/* PEXOCTextColorIndex             10 */
_PEXDecodeColor,		/* PEXOCTextColor                  11 */
_PEXDecodeFloat,		/* PEXOCCharHeight                 12 */
_PEXDecodeVector2D,		/* PEXOCCharUpVector               13 */
_PEXDecodeCARD16,		/* PEXOCTextPath                   14 */
_PEXDecodeTextAlignment,	/* PEXOCTextAlignment              15 */
_PEXDecodeFloat,		/* PEXOCATextHeight                16 */
_PEXDecodeVector2D,		/* PEXOCATextUpVector              17 */
_PEXDecodeCARD16,		/* PEXOCATextPath                  18 */
_PEXDecodeTextAlignment,	/* PEXOCATextAlignment             19 */
_PEXDecodeEnumType,		/* PEXOCATextStyle                 20 */
_PEXDecodeTableIndex,		/* PEXOCTextBundleIndex            21 */
_PEXDecodeEnumType,		/* PEXOCLineType                   22 */
_PEXDecodeFloat,		/* PEXOCLineWidth                  23 */
_PEXDecodeTableIndex,		/* PEXOCLineColorIndex             24 */
_PEXDecodeColor,		/* PEXOCLineColor                  25 */
_PEXDecodeCurveApprox,		/* PEXOCCurveApprox                26 */
_PEXDecodeEnumType,		/* PEXOCPolylineInterpMethod       27 */
_PEXDecodeTableIndex,		/* PEXOCLineBundleIndex            28 */
_PEXDecodeEnumType,		/* PEXOCInteriorStyle              29 */
_PEXDecodeTableIndex,		/* PEXOCInteriorStyleIndex         30 */
_PEXDecodeTableIndex,		/* PEXOCSurfaceColorIndex          31 */
_PEXDecodeColor,		/* PEXOCSurfaceColor               32 */
_PEXDecodeReflectionAttr,	/* PEXOCReflectionAttributes       33 */
_PEXDecodeEnumType,		/* PEXOCReflectionModel            34 */
_PEXDecodeEnumType,		/* PEXOCSurfaceInterpMethod        35 */
_PEXDecodeEnumType,		/* PEXOCBFInteriorStyle            36 */
_PEXDecodeTableIndex,		/* PEXOCBFInteriorStyleIndex       37 */
_PEXDecodeTableIndex,		/* PEXOCBFSurfaceColorIndex        38 */
_PEXDecodeColor,		/* PEXOCBFSurfaceColor             39 */
_PEXDecodeReflectionAttr,	/* PEXOCBFReflectionAttributes     40 */
_PEXDecodeEnumType,		/* PEXOCBFReflectionModel          41 */
_PEXDecodeEnumType,		/* PEXOCBFSurfaceInterpMethod      42 */
_PEXDecodeSurfaceApprox,	/* PEXOCSurfaceApprox              43 */
_PEXDecodeCullMode,		/* PEXOCFacetCullingMode           44 */
_PEXDecodeSwitch,		/* PEXOCFacetDistinguishFlag       45 */
_PEXDecodePatternSize,		/* PEXOCPatternSize                46 */
_PEXDecodePatternAttr2D,	/* PEXOCPatternAttributes2D        47 */
_PEXDecodePatternAttr,		/* PEXOCPatternAttributes          48 */
_PEXDecodeTableIndex,		/* PEXOCInteriorBundleIndex        49 */
_PEXDecodeSwitch,		/* PEXOCSurfaceEdgeFlag            50 */
_PEXDecodeEnumType,		/* PEXOCSurfaceEdgeType            51 */
_PEXDecodeFloat,		/* PEXOCSurfaceEdgeWidth           52 */
_PEXDecodeTableIndex,		/* PEXOCSurfaceEdgeColorIndex      53 */
_PEXDecodeColor,		/* PEXOCSurfaceEdgeColor           54 */
_PEXDecodeTableIndex,		/* PEXOCEdgeBundleIndex            55 */
_PEXDecodeASF,			/* PEXOCIndividualASF              56 */
_PEXDecodeLocalTransform,	/* PEXOCLocalTransform             57 */
_PEXDecodeLocalTransform2D,	/* PEXOCLocalTransform2D           58 */
_PEXDecodeGlobalTransform,	/* PEXOCGlobalTransform            59 */
_PEXDecodeGlobalTransform2D,	/* PEXOCGlobalTransform2D          60 */
_PEXDecodeSwitch,		/* PEXOCModelClipFlag              61 */
_PEXDecodeModelClipVolume,	/* PEXOCModelClipVolume            62 */
_PEXDecodeModelClipVolume2D,	/* PEXOCModelClipVolume2D          63 */
_PEXDecodeRestoreModelClip,	/* PEXOCRestoreModelClipVolume     64 */
_PEXDecodeTableIndex,		/* PEXOCViewIndex                  65 */
_PEXDecodeLightSourceState,	/* PEXOCLightSourceState           66 */
_PEXDecodeTableIndex,		/* PEXOCDepthCueIndex              67 */
_PEXDecodeID,			/* PEXOCPickID                     68 */
_PEXDecodeID,			/* PEXOCHLHSRID                    69 */
_PEXDecodeTableIndex,		/* PEXOCColorApproxIndex           70 */
_PEXDecodeEnumType,		/* PEXOCRenderingColorModel        71 */
_PEXDecodePSC,			/* PEXOCParaSurfCharacteristics    72 */
_PEXDecodeNameSet,		/* PEXOCAddToNameSet               73 */
_PEXDecodeNameSet,		/* PEXOCRemoveFromNameSet          74 */
_PEXDecodeExecuteStructure,	/* PEXOCExecuteStructure           75 */
_PEXDecodeLabel,		/* PEXOCLabel                      76 */
_PEXDecodeApplicationData,	/* PEXOCApplicationData            77 */
_PEXDecodeGSE,			/* PEXOCGSE                        78 */
_PEXDecodeMarkers,		/* PEXOCMarkers                    79 */
_PEXDecodeMarkers2D,		/* PEXOCMarkers2D                  80 */
_PEXDecodeText,			/* PEXOCText                       81 */
_PEXDecodeText2D,		/* PEXOCText2D                     82 */
_PEXDecodeAnnoText,		/* PEXOCAnnotationText             83 */
_PEXDecodeAnnoText2D,		/* PEXOCAnnotationText2D           84 */
_PEXDecodePolyline,		/* PEXOCPolyline                   85 */
_PEXDecodePolyline2D,		/* PEXOCPolyline2D                 86 */
_PEXDecodePolylineSet,		/* PEXOCPolylineSetWithData        87 */
_PEXDecodeNURBCurve,		/* PEXOCNURBCurve                  88 */
_PEXDecodeFillArea,		/* PEXOCFillArea                   89 */
_PEXDecodeFillArea2D,		/* PEXOCFillArea2D                 90 */
_PEXDecodeFillAreaWithData,	/* PEXOCFillAreaWithData           91 */
_PEXDecodeFillAreaSet,		/* PEXOCFillAreaSet                92 */
_PEXDecodeFillAreaSet2D,	/* PEXOCFillAreaSet2D              93 */
_PEXDecodeFillAreaSetWithData,	/* PEXOCFillAreaSetWithData        94 */
_PEXDecodeTriangleStrip,	/* PEXOCTriangleStrip              95 */
_PEXDecodeQuadMesh,		/* PEXOCQuadrilateralMesh          96 */
_PEXDecodeSOFA,			/* PEXOCSetOfFillAreaSets          97 */
_PEXDecodeNURBSurface,		/* PEXOCNURBSurface                98 */
_PEXDecodeCellArray,		/* PEXOCCellArray                  99 */
_PEXDecodeCellArray2D,		/* PEXOCCellArray2D                100 */
_PEXDecodeExtendedCellArray,	/* PEXOCExtendedCellArray          101 */
_PEXDecodeGDP,			/* PEXOCGDP                        102 */
_PEXDecodeGDP2D,		/* PEXOCGDP2D                      103 */
_PEXDecodeNoop			/* PEXOCNoop                       104 */
};

