/* $Xorg: miLvl2Tab.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
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

/* Level II output command handling tables */

#include "mipex.h"


/* procedures for PickExecuteOCTable and SearchExecuteOCTable */

extern	ddpex2rtn	miTestPickGdp3d(),
			miTestPickGdp2d(),
                        miPickAnnoText3D(),
                        miPickAnnoText2D(),
			miPickPrimitives(),
			miTestSearchGdp3d(),
			miTestSearchGdp2d(),
			miSearchPrimitives();

/* procedures for ExecuteOCTable */

extern	ddpex2rtn   miMarkerType(),
		    miMarkerScale(),
		    miMarkerColourOC(),
		    miMarkerBundleIndex(),
		    miTextFontIndex(),
		    miTextPrecision(),
		    miCharExpansion(),
		    miCharSpacing(),
		    miTextColourOC(),
		    miCharHeight(),
		    miCharUpVector(),
		    miTextPath(),
		    miTextAlignment(),
		    miAtextHeight(),
		    miAtextUpVector(),
		    miAtextPath(),
		    miAtextAlignment(),
		    miAtextStyle(),
		    miTextBundleIndex(),
		    miLineType(),
		    miLineWidth(),
		    miLineColourOC(),
		    miCurveApproximation(),
		    miTestSetAttribute(),
		    miLineBundleIndex(),
		    miInteriorStyle(),
		    miTestSetAttribute(),
		    miSurfaceColourOC(),
		    miSurfaceReflAttr(),
		    miSurfaceReflModel(),
		    miSurfaceInterp(),
		    miSurfaceApproximation(),
		    miTestSetAttribute(),
		    miTestColourOC(),
		    miCullingMode(),
		    miInteriorBundleIndex(),
		    miSurfaceEdgeFlag(),
		    miSurfaceEdgeType(),
		    miSurfaceEdgeWidth(),
		    miEdgeColourOC(),
		    miEdgeBundleIndex(),
		    miSetAsfValues(),
		    miLocalTransform(),
		    miLocalTransform2D(),
		    miGlobalTransform(),
		    miGlobalTransform2D(),
		    miModelClip(),
		    miSetMCVolume(),
		    miSetMCVolume(),
		    miRestoreMCV(),
		    miViewIndex(),
		    miLightStateOC(),
		    miPickId(),
		    miColourApproxIndex(),
		    miRenderingColourModel(),
		    miParaSurfCharacteristics(),
		    miAddToNameSet(),
		    miExecuteStructure(),
		    miNoop(),
		    miPolyMarker(),
		    miText3D(),
		    miText2D(),
		    miAnnoText3D(),
		    miAnnoText2D(),
		    miPolyLines(),
		    miNurbsCurve(),
		    miFillArea(),
		    miTriangleStrip(),
		    miQuadMesh(),
		    miSOFAS(),
		    miNurbsSurface(),
		    miCellArray(),
		    miTestExtCellArray(),
		    miTestGDP(),
		    miDepthCueIndex() ;


/* for now, use the old parse routines from dipex.  These use the
 * ocs in PEX format and convert them to the original level III
 * calls and these call the execute procedure directly 
 */

extern ddpex2rtn
	parseColourOC(),
	parseLightState(),
	parseColourIndexOC(),
	parseApplicationData(),
	parseGse(),
	parseMarker(),
	parseMarker2D(),
	parseText(),
	parseText2D(),
	parseAnnotationText(),
	parseAnnotationText2D(),
	parsePolyline(),
	parsePolyline2D(),
	parsePolylineSet(),
	parseNurbCurve(),
	parseFillArea(),
	parseFillArea2D(),
	parseExtFillArea(),
	parseFillAreaSet(),
	parseFillAreaSet2D(),
	parseExtFillAreaSet(),
	parseTriangleStrip(),
	parseQuadrilateralMesh(),
	parseNurbSurface(),
	parseCellArray(),
	parseCellArray2D(),
	parseExtCellArray(),
	parseGdp(),
	parseGdp2D(),
	parseSetAttribute(),
	parsePropOC(),
	parseSOFAS(),
	parsePSurfaceChars(),
	parseSetMCVolume(),
	parseSetMCVolume2D();

ocTableType	ParseOCTable[] = {
    parsePropOC,	        /* 0 dummy entry */
    parseSetAttribute,		/* 1 marker type */
    parseSetAttribute,		/* 2 marker scale */
    parseColourIndexOC,		/* 3 marker colour index */
    parseColourOC,		/* 4 marker colour */
    parseSetAttribute,		/* 5 marker bundle index */
    parseSetAttribute,		/* 6 text font index */
    parseSetAttribute,		/* 7 text precision */
    parseSetAttribute,		/* 8 character expansion */
    parseSetAttribute,		/* 9 character spacing */
    parseColourIndexOC,		/* 10 text colour index */
    parseColourOC,		/* 11 text colour */
    parseSetAttribute,		/* 12 character height */
    parseSetAttribute,		/* 13 character up vector */
    parseSetAttribute,		/* 14 text path */
    parseSetAttribute,		/* 15 text alignment */
    parseSetAttribute,		/* 16 annotation text height */
    parseSetAttribute,		/* 17 annotation text up vector */
    parseSetAttribute,		/* 18 annotation text path */
    parseSetAttribute,		/* 19 annotation text alignment */
    parseSetAttribute,		/* 20 annotation text style */
    parseSetAttribute,		/* 21 text bundle index */
    parseSetAttribute,		/* 22 line type */
    parseSetAttribute,		/* 23 line width */
    parseColourIndexOC,		/* 24 line colour index */
    parseColourOC,		/* 25 line colour */
    parseSetAttribute,		/* 26 curve approximation method */
    parseSetAttribute,		/* 27 polyline interpolation method */
    parseSetAttribute,		/* 28 line bundle index */
    parseSetAttribute,		/* 29 surface interior style */
    parseSetAttribute,		/* 30 surface interior style index */
    parseColourIndexOC,		/* 31 surface colour index */
    parseColourOC,		/* 32 surface colour */
    parseSetAttribute,		/* 33 surface reflection attributes */
    parseSetAttribute,		/* 34 surface reflection model */
    parseSetAttribute,		/* 35 surface interpolation method */
    parseSetAttribute,		/* 36 backface surface interior style */
    parseSetAttribute,		/* 37 backface surface interior style index */
    parseColourIndexOC,		/* 38 backface surface colour index */
    parseColourOC,		/* 39 backface surface colour */
    parseSetAttribute,		/* 40 backface surface reflection
				 *    attributes */
    parseSetAttribute,		/* 41 backface surface reflection model */
    parseSetAttribute,		/* 42 backface surface interpolation method */
    parseSetAttribute,		/* 43 surface approximation */
    parseSetAttribute,		/* 44 facet culling mode */
    parseSetAttribute,		/* 45 facet distinguish flag */
    parseSetAttribute,		/* 46 pattern size */
    parseSetAttribute,		/* 47 pattern reference point */
    parseSetAttribute,		/* 48 pattern reference point and vectors */
    parseSetAttribute,		/* 49 interior bundle index */
    parseSetAttribute,		/* 50 surface edge flag */
    parseSetAttribute,		/* 51 surface edge type */
    parseSetAttribute,		/* 52 surface edge width */
    parseColourIndexOC,		/* 53 surface edge colour index */
    parseColourOC,		/* 54 surface edge colour */
    parseSetAttribute,		/* 55 edge bundle index */
    parseSetAttribute,		/* 56 set individual asf */
    parseSetAttribute,		/* 57 local transform 3d */
    parseSetAttribute,		/* 58 local transform 2d */
    parseSetAttribute,		/* 59 global transform 3d */
    parseSetAttribute,		/* 60 global transform 2d */
    parseSetAttribute,		/* 61 model clip */
    parseSetMCVolume,		/* 62 set model clip volume 3d */
    parseSetMCVolume2D,		/* 63 set model clip volume 2d */
    parseSetAttribute,		/* 64 restore model clip volume */
    parseSetAttribute,		/* 65 view index */
    parseLightState,		/* 66 light source state */
    parseSetAttribute,		/* 67 depth cue index */
    parseSetAttribute,		/* 68 pick id */
    parseSetAttribute,		/* 69 hlhsr identifier */
    parseSetAttribute,		/* 70 colour approx index */
    parseSetAttribute,		/* 71 rendering colour model */
    parsePSurfaceChars,		/* 72 parametric surface attributes */
    parseSetAttribute,		/* 73 add names to name set */
    parseSetAttribute,		/* 74 remove names from name set */
    parseSetAttribute,		/* 75 execute structure */
    parseSetAttribute,		/* 76 label */
    parseSetAttribute,		/* 77 application data */
    parseSetAttribute,		/* 78 gse */
    parseMarker,		/* 79 marker 3d */
    parseMarker2D,		/* 80 marker 2d */
    parseText,			/* 81 text3d */
    parseText2D,		/* 82 text2d */
    parseAnnotationText,	/* 83 annotation text3d */
    parseAnnotationText2D,	/* 84 annotation text2d */
    parsePolyline,		/* 85 polyline3d */
    parsePolyline2D,		/* 86 polyline2d */
    parsePolylineSet,		/* 87 polyline set 3d with data */
    parseNurbCurve,		/* 88 non-uniform b spline curve */
    parseFillArea,		/* 89 fill area 3d */
    parseFillArea2D,		/* 90 fill area 2d */
    parseExtFillArea,		/* 91 fill area 3d with data */
    parseFillAreaSet,		/* 92 fill area set 3d */
    parseFillAreaSet2D,		/* 93 fill area set 2d */
    parseExtFillAreaSet,	/* 94 fill area set 3d with data */
    parseTriangleStrip,		/* 95 triangle strip */
    parseQuadrilateralMesh,	/* 96 quadrilateral mesh */
    parseSOFAS,			/* 97 set of fill area sets */
    parseNurbSurface,		/* 98 non-uniform b spline surface */
    parseCellArray,		/* 99 cell array 3d */
    parseCellArray2D,		/* 100 cell array 2d */
    parseExtCellArray,		/* 101 extended cell array 3d */
    parseGdp,			/* 102 gdp 3d */
    parseGdp2D,			/* 103 gdp 2d */
    parseSetAttribute		/* 104 Noop */
};

extern void		destroyOC_PEX(),
			destroyNoOp(),
			destroySOFAS(),
			destroyNurbSurface();

destroyTableType	DestroyOCTable[] = {
    destroyOC_PEX,		/* 0 entry for proprietary OCs */
    destroyOC_PEX,		/* 1 marker type */
    destroyOC_PEX,		/* 2 marker scale */
    destroyOC_PEX,		/* 3 marker colour index */
    destroyOC_PEX,		/* 4 marker colour */
    destroyOC_PEX,		/* 5 marker bundle index */
    destroyOC_PEX,		/* 6 text font index */
    destroyOC_PEX,		/* 7 text precision */
    destroyOC_PEX,		/* 8 character expansion */
    destroyOC_PEX,		/* 9 character spacing */
    destroyOC_PEX,		/* 10 text colour index */
    destroyOC_PEX,		/* 11 text colour */
    destroyOC_PEX,		/* 12 character height */
    destroyOC_PEX,		/* 13 character up vector */
    destroyOC_PEX,		/* 14 text path */
    destroyOC_PEX,		/* 15 text alignment */
    destroyOC_PEX,		/* 16 annotation text height */
    destroyOC_PEX,		/* 17 annotation text up vector */
    destroyOC_PEX,		/* 18 annotation text path */
    destroyOC_PEX,		/* 19 annotation text alignment */
    destroyOC_PEX,		/* 20 annotation text style */
    destroyOC_PEX,		/* 21 text bundle index */
    destroyOC_PEX,		/* 22 line type */
    destroyOC_PEX,		/* 23 line width */
    destroyOC_PEX,		/* 24 line colour index */
    destroyOC_PEX,		/* 25 line colour */
    destroyOC_PEX,		/* 26 curve approximation method */
    destroyOC_PEX,		/* 27 polyline interpolation method */
    destroyOC_PEX,		/* 28 line bundle index */
    destroyOC_PEX,		/* 29 surface interior style */
    destroyOC_PEX,		/* 30 surface interior style index */
    destroyOC_PEX,		/* 31 surface colour index */
    destroyOC_PEX,		/* 32 surface colour */
    destroyOC_PEX,		/* 33 surface reflection attributes */
    destroyOC_PEX,		/* 34 surface reflection model */
    destroyOC_PEX,		/* 35 surface interpolation method */
    destroyOC_PEX,		/* 36 backface surface interior style */
    destroyOC_PEX,		/* 37 backface surface interior style index */
    destroyOC_PEX,		/* 38 backface surface colour index */
    destroyOC_PEX,		/* 39 backface surface colour */
    destroyOC_PEX,		/* 40 backface surface reflection */
    destroyOC_PEX,		/* 41 backface surface reflection model */
    destroyOC_PEX,		/* 42 backface surface interpolation method */
    destroyOC_PEX,		/* 43 surface approximation */
    destroyOC_PEX,		/* 44 facet culling mode */
    destroyOC_PEX,		/* 45 facet distinguish flag */
    destroyOC_PEX,		/* 46 pattern size */
    destroyOC_PEX,		/* 47 pattern reference point */
    destroyOC_PEX,		/* 48 pattern reference point and vectors */
    destroyOC_PEX,		/* 49 interior bundle index */
    destroyOC_PEX,		/* 50 surface edge flag */
    destroyOC_PEX,		/* 51 surface edge type */
    destroyOC_PEX,		/* 52 surface edge width */
    destroyOC_PEX,		/* 53 surface edge colour index */
    destroyOC_PEX,		/* 54 surface edge colour */
    destroyOC_PEX,		/* 55 edge bundle index */
    destroyOC_PEX,		/* 56 set individual asf */
    destroyOC_PEX,		/* 57 local transform 3d */
    destroyOC_PEX,		/* 58 local transform 2d */
    destroyOC_PEX,		/* 59 global transform 3d */
    destroyOC_PEX,		/* 60 global transform 2d */
    destroyOC_PEX,		/* 61 model clip */
    destroyOC_PEX,		/* 62 destroy model clip volume 3d */
    destroyOC_PEX,		/* 63 destroy model clip volume 2d */
    destroyOC_PEX,		/* 64 restore model clip volume */
    destroyOC_PEX,		/* 65 view index */
    destroyOC_PEX,		/* 66 light source state */
    destroyOC_PEX,		/* 67 depth cue index */
    destroyOC_PEX,		/* 68 pick id */
    destroyOC_PEX,		/* 69 hlhsr identifier */
    destroyOC_PEX,		/* 70 colour approx index */
    destroyOC_PEX,		/* 71 rendering colour model */
    destroyOC_PEX,		/* 72 parametric surface characteristics */
    destroyOC_PEX,		/* 73 add names to name set */
    destroyOC_PEX,		/* 74 remove names from name set */
    destroyOC_PEX,		/* 75 execute structure */
    destroyOC_PEX,		/* 76 label */
    destroyOC_PEX,		/* 77 application data */
    destroyOC_PEX,		/* 78 gse */
    destroyOC_PEX,		/* 79 marker 3d */
    destroyOC_PEX,		/* 80 marker 2d */
    destroyOC_PEX,		/* 81 text3d */
    destroyOC_PEX,		/* 82 text2d */
    destroyOC_PEX,		/* 83 annotation text3d */
    destroyOC_PEX,		/* 84 annotation text2d */
    destroyOC_PEX,		/* 85 polyline3d */
    destroyOC_PEX,		/* 86 polyline2d */
    destroyOC_PEX,		/* 87 polyline set 3d with data */
    destroyOC_PEX,		/* 88 non-uniform b spline curve */
    destroyOC_PEX,		/* 89 fill area 3d */
    destroyOC_PEX,		/* 90 fill area 2d */
    destroyOC_PEX,		/* 91 fill area 3d with data */
    destroyOC_PEX,		/* 92 fill area set 3d */
    destroyOC_PEX,		/* 93 fill area set 2d */
    destroyOC_PEX,	        /* 94 fill area set 3d with data */
    destroyOC_PEX,		/* 95 triangle strip */
    destroyOC_PEX,        	/* 96 quadrilateral mesh */
    destroySOFAS,		/* 97 set of fill area sets */
    destroyNurbSurface,		/* 98 non-uniform b spline surface */
    destroyOC_PEX,		/* 99 cell array 3d */
    destroyOC_PEX,		/* 100 cell array 2d */
    destroyOC_PEX,		/* 101 extended cell array 3d */
    destroyOC_PEX,		/* 102 gdp 3d */
    destroyOC_PEX,		/* 103 gdp 2d */
    destroyOC_PEX		/* 104 Noop */
};

/* initial setup for output command table in renderers */

ocTableType	InitExecuteOCTable[] = {
    miNoop,		    /* 0 dummy entry */
    miMarkerType,	    /* 1 marker type */
    miMarkerScale,	    /* 2 marker scale */
    miMarkerColourOC,	    /* 3 marker colour index */
    miMarkerColourOC,	    /* 4 marker colour */
    miMarkerBundleIndex,    /* 5 marker bundle index */
    miTextFontIndex,	    /* 6 text font index */
    miTextPrecision,	    /* 7 text precision */
    miCharExpansion,	    /* 8 character expansion */
    miCharSpacing,	    /* 9 character spacing */
    miTextColourOC,	    /* 10 text colour index */
    miTextColourOC,	    /* 11 text colour */
    miCharHeight,	    /* 12 character height */
    miCharUpVector,	    /* 13 character up vector */
    miTextPath,		    /* 14 text path */
    miTextAlignment,	    /* 15 text alignment */
    miAtextHeight,	    /* 16 annotation text height */
    miAtextUpVector,	    /* 17 annotation text up vector */
    miAtextPath,	    /* 18 annotation text path */
    miAtextAlignment,	    /* 19 annotation text alignment */
    miAtextStyle,	    /* 20 annotation text style */
    miTextBundleIndex,	    /* 21 text bundle index */
    miLineType,		    /* 22 line type */
    miLineWidth,	    /* 23 line width */
    miLineColourOC,	    /* 24 line colour index */
    miLineColourOC,	    /* 25 line colour */
    miCurveApproximation,   /* 26 curve approximation method */
    miTestSetAttribute,	    /* 27 polyline interpolation method */
    miLineBundleIndex,	    /* 28 line bundle index */
    miInteriorStyle,	    /* 29 surface interior style */
    miTestSetAttribute,	    /* 30 surface interior style index */
    miSurfaceColourOC,	    /* 31 surface colour index */
    miSurfaceColourOC,	    /* 32 surface colour */
    miSurfaceReflAttr,	    /* 33 surface reflection attributes */
    miSurfaceReflModel,	    /* 34 surface reflection model */
    miSurfaceInterp,	    /* 35 surface interpolation method */
    miTestSetAttribute,	    /* 36 backface surface interior style */
    miTestSetAttribute,	    /* 37 backface surface interior style index */
    miTestColourOC,	    /* 38 backface surface colour index */
    miTestColourOC,	    /* 39 backface surface colour */
    miTestSetAttribute,	    /* 40 backface surface reflection attributes */
    miTestSetAttribute,	    /* 41 backface surface reflection model */
    miTestSetAttribute,	    /* 42 backface surface interpolation method */
    miSurfaceApproximation, /* 43 surface approximation */
    miCullingMode,	    /* 44 facet culling mode */
    miTestSetAttribute,	    /* 45 facet distinguish flag */
    miTestSetAttribute,	    /* 46 pattern size */
    miTestSetAttribute,	    /* 47 pattern reference point */
    miTestSetAttribute,	    /* 48 pattern reference point and vectors */
    miInteriorBundleIndex,  /* 49 interior bundle index */
    miSurfaceEdgeFlag,	    /* 50 surface edge flag */
    miSurfaceEdgeType,	    /* 51 surface edge type */
    miSurfaceEdgeWidth,	    /* 52 surface edge width */
    miEdgeColourOC,	    /* 53 surface edge colour index */
    miEdgeColourOC,	    /* 54 surface edge colour */
    miEdgeBundleIndex,	    /* 55 edge bundle index */
    miSetAsfValues,	    /* 56 set individual asf */
    miLocalTransform,	    /* 57 local transform 3d */
    miLocalTransform2D,	    /* 58 local transform 2d */
    miGlobalTransform,	    /* 59 global transform 3d */
    miGlobalTransform2D,    /* 60 global transform 2d */
    miModelClip,	    /* 61 model clip */
    miSetMCVolume,	    /* 62 set model clip volume 3d */
    miSetMCVolume,	    /* 63 set model clip volume 2d */
    miRestoreMCV,	    /* 64 restore model clip volume */
    miViewIndex,	    /* 65 view index */
    miLightStateOC,	    /* 66 light source state */
    miDepthCueIndex,	    /* 67 depth cue index */
    miPickId,		    /* 68 pick id */
    miTestSetAttribute,	    /* 69 hlhsr identifier */
    miColourApproxIndex,    /* 70 colour approx index */
    miRenderingColourModel, /* 71 rendering colour model */
    miParaSurfCharacteristics, /* 72 parametric surface characteristics */
    miAddToNameSet,	    /* 73 add names to name set */
    miAddToNameSet,	    /* 74 remove names from name set */
    miExecuteStructure,	    /* 75 execute structure */
    miNoop,		    /* 76 label */
    miNoop,		    /* 77 application data */
    miNoop,		    /* 78 gse */
    miPolyMarker,	    /* 79 marker 3d */
    miPolyMarker,	    /* 80 marker 2d */
    miText3D,		    /* 81 text3d */
    miText2D,		    /* 82 text2d */
    miAnnoText3D,	    /* 83 annotation text3d */
    miAnnoText2D,	    /* 84 annotation text2d */
    miPolyLines,	    /* 85 polyline3d */
    miPolyLines,	    /* 86 polyline2d */
    miPolyLines,	    /* 87 polyline set 3d with data */
    miNurbsCurve,	    /* 88 non-uniform b spline curve */
    miFillArea,		    /* 89 fill area 3d */
    miFillArea,		    /* 90 fill area 2d */
    miFillArea,		    /* 91 fill area 3d with data */
    miFillArea,		    /* 92 fill area set 3d */
    miFillArea,		    /* 93 fill area set 2d */
    miFillArea,		    /* 94 fill area set 3d with data */
    miTriangleStrip,	    /* 95 triangle strip */
    miQuadMesh,		    /* 96 quadrilateral mesh */
    miSOFAS,		    /* 97 set of fill area sets */
    miNurbsSurface,	    /* 98 non-uniform b spline surface */
    miCellArray,	    /* 99 cell array 3d */
    miCellArray,	    /* 100 cell array 2d */
    miCellArray,	    /* 101 extended cell array 3d */
    miTestGDP,		    /* 102 gdp 3d */
    miTestGDP,		    /* 103 gdp 2d */
    miNoop		    /* 104 Noop */
};

/* initial setup for output command table for picking */

ocTableType	PickExecuteOCTable[] = {
    miNoop,		    /* 0 dummy entry */
    miMarkerType,	    /* 1 marker type */
    miMarkerScale,	    /* 2 marker scale */
    miMarkerColourOC,	    /* 3 marker colour index */
    miMarkerColourOC,	    /* 4 marker colour */
    miMarkerBundleIndex,    /* 5 marker bundle index */
    miTextFontIndex,	    /* 6 text font index */
    miTextPrecision,	    /* 7 text precision */
    miCharExpansion,	    /* 8 character expansion */
    miCharSpacing,	    /* 9 character spacing */
    miTextColourOC,	    /* 10 text colour index */
    miTextColourOC,	    /* 11 text colour */
    miCharHeight,	    /* 12 character height */
    miCharUpVector,	    /* 13 character up vector */
    miTextPath,		    /* 14 text path */
    miTextAlignment,	    /* 15 text alignment */
    miAtextHeight,	    /* 16 annotation text height */
    miAtextUpVector,	    /* 17 annotation text up vector */
    miAtextPath,	    /* 18 annotation text path */
    miAtextAlignment,	    /* 19 annotation text alignment */
    miAtextStyle,	    /* 20 annotation text style */
    miTextBundleIndex,	    /* 21 text bundle index */
    miLineType,		    /* 22 line type */
    miLineWidth,	    /* 23 line width */
    miLineColourOC,	    /* 24 line colour index */
    miLineColourOC,	    /* 25 line colour */
    miCurveApproximation,   /* 26 curve approximation method */
    miTestSetAttribute,	    /* 27 polyline interpolation method */
    miLineBundleIndex,	    /* 28 line bundle index */
    miInteriorStyle,	    /* 29 surface interior style */
    miTestSetAttribute,	    /* 30 surface interior style index */
    miSurfaceColourOC,	    /* 31 surface colour index */
    miSurfaceColourOC,	    /* 32 surface colour */
    miSurfaceReflAttr,	    /* 33 surface reflection attributes */
    miSurfaceReflModel,	    /* 34 surface reflection model */
    miSurfaceInterp,	    /* 35 surface interpolation method */
    miTestSetAttribute,	    /* 36 backface surface interior style */
    miTestSetAttribute,	    /* 37 backface surface interior style index */
    miTestColourOC,	    /* 38 backface surface colour index */
    miTestColourOC,	    /* 39 backface surface colour */
    miTestSetAttribute,	    /* 40 backface surface reflection attributes */
    miTestSetAttribute,	    /* 41 backface surface reflection model */
    miTestSetAttribute,	    /* 42 backface surface interpolation method */
    miSurfaceApproximation, /* 43 surface approximation */
    miCullingMode,	    /* 44 facet culling mode */
    miTestSetAttribute,	    /* 45 facet distinguish flag */
    miTestSetAttribute,	    /* 46 pattern size */
    miTestSetAttribute,	    /* 47 pattern reference point */
    miTestSetAttribute,	    /* 48 pattern reference point and vectors */
    miInteriorBundleIndex,  /* 49 interior bundle index */
    miSurfaceEdgeFlag,	    /* 50 surface edge flag */
    miSurfaceEdgeType,	    /* 51 surface edge type */
    miSurfaceEdgeWidth,	    /* 52 surface edge width */
    miEdgeColourOC,	    /* 53 surface edge colour index */
    miEdgeColourOC,	    /* 54 surface edge colour */
    miEdgeBundleIndex,	    /* 55 edge bundle index */
    miSetAsfValues,	    /* 56 set individual asf */
    miLocalTransform,	    /* 57 local transform 3d */
    miLocalTransform2D,	    /* 58 local transform 2d */
    miGlobalTransform,	    /* 59 global transform 3d */
    miGlobalTransform2D,    /* 60 global transform 2d */
    miModelClip,	    /* 61 model clip */
    miSetMCVolume,	    /* 62 set model clip volume 3d */
    miSetMCVolume,	    /* 63 set model clip volume 2d */
    miRestoreMCV,	    /* 64 restore model clip volume */
    miViewIndex,	    /* 65 view index */
    miLightStateOC,	    /* 66 light source state */
    miDepthCueIndex,	    /* 67 depth cue index */
    miPickId,		    /* 68 pick id */
    miTestSetAttribute,	    /* 69 hlhsr identifier */
    miColourApproxIndex,    /* 70 colour approx index */
    miRenderingColourModel, /* 71 rendering colour model */
    miParaSurfCharacteristics, /* 72 parametric surface characteristics */
    miAddToNameSet,	    /* 73 add names to name set */
    miAddToNameSet,	    /* 74 remove names from name set */
    miExecuteStructure,	    /* 75 execute structure */
    miNoop,		    /* 76 label */
    miNoop,		    /* 77 application data */
    miNoop,		    /* 78 gse */
    miPickPrimitives,	    /* 79 marker 3d */
    miPickPrimitives,	    /* 80 marker 2d */
    miPickPrimitives,	    /* 81 text3d */
    miPickPrimitives,	    /* 82 text2d */
    miPickAnnoText3D,	    /* 83 annotation text3d */
    miPickAnnoText2D,	    /* 84 annotation text2d */
    miPickPrimitives,	    /* 85 polyline3d */
    miPickPrimitives,	    /* 86 polyline2d */
    miPickPrimitives,	    /* 87 polyline set 3d with data */
    miPickPrimitives,	    /* 88 non-uniform b spline curve */
    miPickPrimitives,	    /* 89 fill area 3d */
    miPickPrimitives,	    /* 90 fill area 2d */
    miPickPrimitives,	    /* 91 fill area 3d with data */
    miPickPrimitives,	    /* 92 fill area set 3d */
    miPickPrimitives,	    /* 93 fill area set 2d */
    miPickPrimitives,	    /* 94 fill area set 3d with data */
    miPickPrimitives,	    /* 95 triangle strip */
    miPickPrimitives,	    /* 96 quadrilateral mesh */
    miPickPrimitives,	    /* 97 set of fill area sets */
    miPickPrimitives,       /* 98 non-uniform b spline surface */
    miPickPrimitives,	    /* 99 cell array 3d */
    miPickPrimitives,	    /* 100 cell array 2d */
    miPickPrimitives,	    /* 101 extended cell array 3d */
    miTestPickGdp3d,	    /* 102 gdp 3d */
    miTestPickGdp2d,	    /* 103 gdp 2d */
    miNoop		    /* 104 Noop  */
};


/* initial setup for output command table for searching */

ocTableType	SearchExecuteOCTable[] = {
    miNoop,		    /* 0 dummy entry */
    miMarkerType,	    /* 1 marker type */
    miMarkerScale,	    /* 2 marker scale */
    miMarkerColourOC,	    /* 3 marker colour index */
    miMarkerColourOC,	    /* 4 marker colour */
    miMarkerBundleIndex,    /* 5 marker bundle index */
    miTextFontIndex,	    /* 6 text font index */
    miTextPrecision,	    /* 7 text precision */
    miCharExpansion,	    /* 8 character expansion */
    miCharSpacing,	    /* 9 character spacing */
    miTextColourOC,	    /* 10 text colour index */
    miTextColourOC,	    /* 11 text colour */
    miCharHeight,	    /* 12 character height */
    miCharUpVector,	    /* 13 character up vector */
    miTextPath,		    /* 14 text path */
    miTextAlignment,	    /* 15 text alignment */
    miAtextHeight,	    /* 16 annotation text height */
    miAtextUpVector,	    /* 17 annotation text up vector */
    miAtextPath,	    /* 18 annotation text path */
    miAtextAlignment,	    /* 19 annotation text alignment */
    miAtextStyle,	    /* 20 annotation text style */
    miTextBundleIndex,	    /* 21 text bundle index */
    miLineType,		    /* 22 line type */
    miLineWidth,	    /* 23 line width */
    miLineColourOC,	    /* 24 line colour index */
    miLineColourOC,	    /* 25 line colour */
    miCurveApproximation,   /* 26 curve approximation method */
    miTestSetAttribute,	    /* 27 polyline interpolation method */
    miLineBundleIndex,	    /* 28 line bundle index */
    miInteriorStyle,	    /* 29 surface interior style */
    miTestSetAttribute,	    /* 30 surface interior style index */
    miSurfaceColourOC,	    /* 31 surface colour index */
    miSurfaceColourOC,	    /* 32 surface colour */
    miSurfaceReflAttr,	    /* 33 surface reflection attributes */
    miSurfaceReflModel,	    /* 34 surface reflection model */
    miSurfaceInterp,	    /* 35 surface interpolation method */
    miTestSetAttribute,	    /* 36 backface surface interior style */
    miTestSetAttribute,	    /* 37 backface surface interior style index */
    miTestColourOC,	    /* 38 backface surface colour index */
    miTestColourOC,	    /* 39 backface surface colour */
    miTestSetAttribute,	    /* 40 backface surface reflection attributes */
    miTestSetAttribute,	    /* 41 backface surface reflection model */
    miTestSetAttribute,	    /* 42 backface surface interpolation method */
    miSurfaceApproximation, /* 43 surface approximation */
    miCullingMode,	    /* 44 facet culling mode */
    miTestSetAttribute,	    /* 45 facet distinguish flag */
    miTestSetAttribute,	    /* 46 pattern size */
    miTestSetAttribute,	    /* 47 pattern reference point */
    miTestSetAttribute,	    /* 48 pattern reference point and vectors */
    miInteriorBundleIndex,  /* 49 interior bundle index */
    miSurfaceEdgeFlag,	    /* 50 surface edge flag */
    miSurfaceEdgeType,	    /* 51 surface edge type */
    miSurfaceEdgeWidth,	    /* 52 surface edge width */
    miEdgeColourOC,	    /* 53 surface edge colour index */
    miEdgeColourOC,	    /* 54 surface edge colour */
    miEdgeBundleIndex,	    /* 55 edge bundle index */
    miSetAsfValues,	    /* 56 set individual asf */
    miLocalTransform,	    /* 57 local transform 3d */
    miLocalTransform2D,	    /* 58 local transform 2d */
    miGlobalTransform,	    /* 59 global transform 3d */
    miGlobalTransform2D,    /* 60 global transform 2d */
    miModelClip,	    /* 61 model clip */
    miSetMCVolume,	    /* 62 set model clip volume 3d */
    miSetMCVolume,	    /* 63 set model clip volume 2d */
    miRestoreMCV,	    /* 64 restore model clip volume */
    miViewIndex,	    /* 65 view index */
    miLightStateOC,	    /* 66 light source state */
    miDepthCueIndex,	    /* 67 depth cue index */
    miPickId,		    /* 68 pick id */
    miTestSetAttribute,	    /* 69 hlhsr identifier */
    miColourApproxIndex,    /* 70 colour approx index */
    miRenderingColourModel, /* 71 rendering colour model */
    miParaSurfCharacteristics, /* 72 parametric surface characteristics */
    miAddToNameSet,	    /* 73 add names to name set */
    miAddToNameSet,	    /* 74 remove names from name set */
    miExecuteStructure,	    /* 75 execute structure */
    miNoop,		    /* 76 label */
    miNoop,		    /* 77 application data */
    miNoop,		    /* 78 gse */
    miSearchPrimitives,	    /* 79 marker 3d */
    miSearchPrimitives,	    /* 80 marker 2d */
    miSearchPrimitives,	    /* 81 text3d */
    miSearchPrimitives,	    /* 82 text2d */
    miSearchPrimitives,	    /* 83 annotation text3d */
    miSearchPrimitives,	    /* 84 annotation text2d */
    miSearchPrimitives,	    /* 85 polyline3d */
    miSearchPrimitives,	    /* 86 polyline2d */
    miSearchPrimitives,	    /* 87 polyline set 3d with data */
    miSearchPrimitives,	    /* 88 non-uniform b spline curve */
    miSearchPrimitives,	    /* 89 fill area 3d */
    miSearchPrimitives,	    /* 90 fill area 2d */
    miSearchPrimitives,	    /* 91 fill area 3d with data */
    miSearchPrimitives,	    /* 92 fill area set 3d */
    miSearchPrimitives,	    /* 93 fill area set 2d */
    miSearchPrimitives,	    /* 94 fill area set 3d with data */
    miSearchPrimitives,	    /* 95 triangle strip */
    miSearchPrimitives,	    /* 96 quadrilateral mesh */
    miSearchPrimitives,	    /* 97 set of fill area sets */
    miSearchPrimitives,     /* 98 non-uniform b spline surface */
    miSearchPrimitives,	    /* 99 cell array 3d */
    miSearchPrimitives,	    /* 100 cell array 2d */
    miSearchPrimitives,	    /* 101 extended cell array 3d */
    miTestSearchGdp3d,	    /* 102 gdp 3d */
    miTestSearchGdp2d,	    /* 103 gdp 2d */
    miNoop		    /* 104 Noop */
};


extern ddpex2rtn
    copyAnnotationText(),
    copyAnnotationText2D(),
    copyCellArray(),
    copyCellArray2D(),
    copyColourIndexOC(),
    copyColourOC(),
    copyExtCellArray(),
    copyExtFillArea(),
    copyExtFillAreaSet(),
    copyFillArea(),
    copyFillArea2D(),
    copyFillAreaSet(),
    copyFillAreaSet2D(),
    copyGdp(),
    copyGdp2D(),
    copyLightState(),
    copyMarker(),
    copyMarker2D(),
    copyNurbCurve(),
    copyNurbSurface(),
    copyPolyline(),
    copyPolyline2D(),
    copyPolylineSet(),
    copyPSurfaceChars(),
    copyMCVolume(),
    copyQuadrilateralMesh(),
    copySetAttribute(),
    copyPropOC(),
    copySOFAS(),
    copyText(),
    copyText2D(),
    copyTriangleStrip();



ocTableType	CopyOCTable[] = {
    copyPropOC,			/* 0 dummy entry */
    copySetAttribute,		/* 1 marker type */
    copySetAttribute,		/* 2 marker scale */
    copyColourIndexOC,		/* 3 marker colour index */
    copyColourOC,		/* 4 marker colour */
    copySetAttribute,		/* 5 marker bundle index */
    copySetAttribute,		/* 6 text font index */
    copySetAttribute,		/* 7 text precision */
    copySetAttribute,		/* 8 character expansion */
    copySetAttribute,		/* 9 character spacing */
    copyColourIndexOC,		/* 10 text colour index */
    copyColourOC,		/* 11 text colour */
    copySetAttribute,		/* 12 character height */
    copySetAttribute,		/* 13 character up vector */
    copySetAttribute,		/* 14 text path */
    copySetAttribute,		/* 15 text alignment */
    copySetAttribute,		/* 16 annotation text height */
    copySetAttribute,		/* 17 annotation text up vector */
    copySetAttribute,		/* 18 annotation text path */
    copySetAttribute,		/* 19 annotation text alignment */
    copySetAttribute,		/* 20 annotation text style */
    copySetAttribute,		/* 21 text bundle index */
    copySetAttribute,		/* 22 line type */
    copySetAttribute,		/* 23 line width */
    copyColourIndexOC,		/* 24 line colour index */
    copyColourOC,		/* 25 line colour */
    copySetAttribute,		/* 26 curve approximation method */
    copySetAttribute,		/* 27 polyline interpolation method */
    copySetAttribute,		/* 28 line bundle index */
    copySetAttribute,		/* 29 surface interior style */
    copySetAttribute,		/* 30 surface interior style index */
    copyColourIndexOC,		/* 31 surface colour index */
    copyColourOC,		/* 32 surface colour */
    copySetAttribute,		/* 33 surface reflection attributes */
    copySetAttribute,		/* 34 surface reflection model */
    copySetAttribute,		/* 35 surface interpolation method */
    copySetAttribute,		/* 36 backface surface interior style */
    copySetAttribute,		/* 37 backface surface interior style index */
    copyColourIndexOC,		/* 38 backface surface colour index */
    copyColourOC,		/* 39 backface surface colour */
    copySetAttribute,		/* 40 backface surface reflection
				 *    attributes */
    copySetAttribute,		/* 41 backface surface reflection model */
    copySetAttribute,		/* 42 backface surface interpolation method */
    copySetAttribute,		/* 43 surface approximation */
    copySetAttribute,		/* 44 facet culling mode */
    copySetAttribute,		/* 45 facet distinguish flag */
    copySetAttribute,		/* 46 pattern size */
    copySetAttribute,		/* 47 pattern reference point */
    copySetAttribute,		/* 48 pattern reference point and vectors */
    copySetAttribute,		/* 49 interior bundle index */
    copySetAttribute,		/* 50 surface edge flag */
    copySetAttribute,		/* 51 surface edge type */
    copySetAttribute,		/* 52 surface edge width */
    copyColourIndexOC,		/* 53 surface edge colour index */
    copyColourOC,		/* 54 surface edge colour */
    copySetAttribute,		/* 55 edge bundle index */
    copySetAttribute,		/* 56 set individual asf */
    copySetAttribute,		/* 57 local transform 3d */
    copySetAttribute,		/* 58 local transform 2d */
    copySetAttribute,		/* 59 global transform 3d */
    copySetAttribute,		/* 60 global transform 2d */
    copySetAttribute,		/* 61 model clip */
    copyMCVolume,		/* 62 copy model clip volume 3d */
    copyMCVolume,		/* 63 copy model clip volume 2d */
    copySetAttribute,		/* 64 restore model clip volume */
    copySetAttribute,		/* 65 view index */
    copyLightState,		/* 66 light source state */
    copySetAttribute,		/* 67 depth cue index */
    copySetAttribute,		/* 68 pick id */
    copySetAttribute,		/* 69 hlhsr identifier */
    copySetAttribute,		/* 70 colour approx index */
    copySetAttribute,		/* 71 rendering colour model */
    copyPSurfaceChars,		/* 72 parametric surface attributes */
    copySetAttribute,		/* 73 add names to name set */
    copySetAttribute,		/* 74 remove names from name set */
    copySetAttribute,		/* 75 execute structure */
    copySetAttribute,		/* 76 label */
    copySetAttribute,		/* 77 application data */
    copySetAttribute,		/* 78 gse */
    copyMarker,			/* 79 marker 3d */
    copyMarker2D,		/* 80 marker 2d */
    copyText,			/* 81 text3d */
    copyText2D,			/* 82 text2d */
    copyAnnotationText,		/* 83 annotation text3d */
    copyAnnotationText2D,	/* 84 annotation text2d */
    copyPolyline,		/* 85 polyline3d */
    copyPolyline2D,		/* 86 polyline2d */
    copyPolylineSet,		/* 87 polyline set 3d with data */
    copyNurbCurve,		/* 88 non-uniform b spline curve */
    copyFillArea,		/* 89 fill area 3d */
    copyFillArea2D,		/* 90 fill area 2d */
    copyExtFillArea,		/* 91 fill area 3d with data */
    copyFillAreaSet,		/* 92 fill area set 3d */
    copyFillAreaSet2D,		/* 93 fill area set 2d */
    copyExtFillAreaSet,		/* 94 fill area set 3d with data */
    copyTriangleStrip,		/* 95 triangle strip */
    copyQuadrilateralMesh,	/* 96 quadrilateral mesh */
    copySOFAS,			/* 97 set of fill area sets */
    copyNurbSurface,		/* 98 non-uniform b spline surface */
    copyCellArray,		/* 99 cell array 3d */
    copyCellArray2D,		/* 100 cell array 2d */
    copyExtCellArray,		/* 101 extended cell array 3d */
    copyGdp,			/* 102 gdp 3d */
    copyGdp2D,			/* 103 gdp 2d */
    copySetAttribute		/* 104 Noop */
};


extern ddpex2rtn
    inquireAnnotationText(),
    inquireAnnotationText2D(),
    inquireCellArray(),
    inquireCellArray2D(),
    inquireColourIndexOC(),
    inquireColourOC(),
    inquireExtCellArray(),
    inquireExtFillArea(),
    inquireExtFillAreaSet(),
    inquireFillArea(),
    inquireFillArea2D(),
    inquireFillAreaSet(),
    inquireFillAreaSet2D(),
    inquireGdp(),
    inquireGdp2D(),
    inquireLightState(),
    inquireMarker(),
    inquireMarker2D(),
    inquireMCVolume(),
    inquireMCVolume2D(),
    inquireNurbCurve(),
    inquireNurbSurface(),
    inquirePolyline(),
    inquirePolyline2D(),
    inquirePolylineSet(),
    inquirePSurfaceChars(),
    inquireQuadrilateralMesh(),
    inquireSetAttribute(),
    inquirePropOC(),
    inquireSOFAS(),
    inquireText(),
    inquireText2D(),
    inquireTriangleStrip(),
    inquireMCVolume(),
    inquireMCVolume2D();


ocTableType	InquireOCTable[] = {
    inquirePropOC,		/* 0 dummy entry */
    inquireSetAttribute,	/* 1 marker type */
    inquireSetAttribute,	/* 2 marker scale */
    inquireColourIndexOC,	/* 3 marker colour index */
    inquireColourOC,		/* 4 marker colour */
    inquireSetAttribute,	/* 5 marker bundle index */
    inquireSetAttribute,	/* 6 text font index */
    inquireSetAttribute,	/* 7 text precision */
    inquireSetAttribute,	/* 8 character expansion */
    inquireSetAttribute,	/* 9 character spacing */
    inquireColourIndexOC,	/* 10 text colour index */
    inquireColourOC,		/* 11 text colour */
    inquireSetAttribute,	/* 12 character height */
    inquireSetAttribute,	/* 13 character up vector */
    inquireSetAttribute,	/* 14 text path */
    inquireSetAttribute,	/* 15 text alignment */
    inquireSetAttribute,	/* 16 annotation text height */
    inquireSetAttribute,	/* 17 annotation text up vector */
    inquireSetAttribute,	/* 18 annotation text path */
    inquireSetAttribute,	/* 19 annotation text alignment */
    inquireSetAttribute,	/* 20 annotation text style */
    inquireSetAttribute,	/* 21 text bundle index */
    inquireSetAttribute,	/* 22 line type */
    inquireSetAttribute,	/* 23 line width */
    inquireColourIndexOC,	/* 24 line colour index */
    inquireColourOC,		/* 25 line colour */
    inquireSetAttribute,	/* 26 curve approximation method */
    inquireSetAttribute,	/* 27 polyline interpolation method */
    inquireSetAttribute,	/* 28 line bundle index */
    inquireSetAttribute,	/* 29 surface interior style */
    inquireSetAttribute,	/* 30 surface interior style index */
    inquireColourIndexOC,	/* 31 surface colour index */
    inquireColourOC,		/* 32 surface colour */
    inquireSetAttribute,	/* 33 surface reflection attributes */
    inquireSetAttribute,	/* 34 surface reflection model */
    inquireSetAttribute,	/* 35 surface interpolation method */
    inquireSetAttribute,	/* 36 backface surface interior style */
    inquireSetAttribute,	/* 37 backface surface interior style index */
    inquireColourIndexOC,	/* 38 backface surface colour index */
    inquireColourOC,		/* 39 backface surface colour */
    inquireSetAttribute,	/* 40 backface surface reflection attributes */
    inquireSetAttribute,	/* 41 backface surface reflection model */
    inquireSetAttribute,	/* 42 backface surface interpolation method */
    inquireSetAttribute,	/* 43 surface approximation */
    inquireSetAttribute,	/* 44 facet culling mode */
    inquireSetAttribute,	/* 45 facet distinguish flag */
    inquireSetAttribute,	/* 46 pattern size */
    inquireSetAttribute,	/* 47 pattern reference point */
    inquireSetAttribute,	/* 48 pattern reference point and vectors */
    inquireSetAttribute,	/* 49 interior bundle index */
    inquireSetAttribute,	/* 50 surface edge flag */
    inquireSetAttribute,	/* 51 surface edge type */
    inquireSetAttribute,	/* 52 surface edge width */
    inquireColourIndexOC,	/* 53 surface edge colour index */
    inquireColourOC,		/* 54 surface edge colour */
    inquireSetAttribute,	/* 55 edge bundle index */
    inquireSetAttribute,	/* 56 set individual asf */
    inquireSetAttribute,	/* 57 local transform 3d */
    inquireSetAttribute,	/* 58 local transform 2d */
    inquireSetAttribute,	/* 59 global transform 3d */
    inquireSetAttribute,	/* 60 global transform 2d */
    inquireSetAttribute,	/* 61 model clip */
    inquireMCVolume,		/* 62 model clip volume 3d */
    inquireMCVolume2D,		/* 63 model clip volume 2d */
    inquireSetAttribute,	/* 64 restore model clip volume */
    inquireSetAttribute,	/* 65 view index */
    inquireLightState,		/* 66 light source state */
    inquireSetAttribute,	/* 67 depth cue index */
    inquireSetAttribute,	/* 68 pick id */
    inquireSetAttribute,	/* 69 hlhsr identifier */
    inquireSetAttribute,	/* 70 colour approx index */
    inquireSetAttribute,	/* 71 rendering colour model */
    inquirePSurfaceChars,	/* 72 parametric surface attributes */
    inquireSetAttribute,	/* 73 add names to name set */
    inquireSetAttribute,	/* 74 remove names from name set */
    inquireSetAttribute,	/* 75 execute structure */
    inquireSetAttribute,	/* 76 label */
    inquireSetAttribute,	/* 77 application data */
    inquireSetAttribute,	/* 78 gse */
    inquireMarker,		/* 79 marker 3d */
    inquireMarker2D,		/* 80 marker 2d */
    inquireText,		/* 81 text3d */
    inquireText2D,		/* 82 text2d */
    inquireAnnotationText,	/* 83 annotation text3d */
    inquireAnnotationText2D,	/* 84 annotation text2d */
    inquirePolyline,		/* 85 polyline3d */
    inquirePolyline2D,		/* 86 polyline2d */
    inquirePolylineSet,		/* 87 polyline set 3d with data */
    inquireNurbCurve,		/* 88 non-uniform b spline curve */
    inquireFillArea,		/* 89 fill area 3d */
    inquireFillArea2D,		/* 90 fill area 2d */
    inquireExtFillArea,		/* 91 fill area 3d with data */
    inquireFillAreaSet,		/* 92 fill area set 3d */
    inquireFillAreaSet2D,	/* 93 fill area set 2d */
    inquireExtFillAreaSet,	/* 94 fill area set 3d with data */
    inquireTriangleStrip,	/* 95 triangle strip */
    inquireQuadrilateralMesh,	/* 96 quadrilateral mesh */
    inquireSOFAS,		/* 97 set of fill area sets */
    inquireNurbSurface,		/* 98 non-uniform b spline surface */
    inquireCellArray,		/* 99 cell array 3d */
    inquireCellArray2D,		/* 100 cell array 2d */
    inquireExtCellArray,	/* 101 extended cell array 3d */
    inquireGdp,			/* 102 gdp 3d */
    inquireGdp2D,		/* 103 gdp 2d */
    inquireSetAttribute		/* 104 Noop */
};



extern ddpex2rtn
    replaceNurbSurface(),
    replaceLightState(),
    replaceSOFAS();


ocTableType	ReplaceOCTable[] = {
    parsePropOC,		/* 0 dummy entry */
    parseSetAttribute,		/* 1 marker type */
    parseSetAttribute,		/* 2 marker scale */
    parseColourIndexOC,		/* 3 marker colour index */
    parseColourOC,		/* 4 marker colour */
    parseSetAttribute,		/* 5 marker bundle index */
    parseSetAttribute,		/* 6 text font index */
    parseSetAttribute,		/* 7 text precision */
    parseSetAttribute,		/* 8 character expansion */
    parseSetAttribute,		/* 9 character spacing */
    parseColourIndexOC,		/* 10 text colour index */
    parseColourOC,		/* 11 text colour */
    parseSetAttribute,		/* 12 character height */
    parseSetAttribute,		/* 13 character up vector */
    parseSetAttribute,		/* 14 text path */
    parseSetAttribute,		/* 15 text alignment */
    parseSetAttribute,		/* 16 annotation text height */
    parseSetAttribute,		/* 17 annotation text up vector */
    parseSetAttribute,		/* 18 annotation text path */
    parseSetAttribute,		/* 19 annotation text alignment */
    parseSetAttribute,		/* 20 annotation text style */
    parseSetAttribute,		/* 21 text bundle index */
    parseSetAttribute,		/* 22 line type */
    parseSetAttribute,		/* 23 line width */
    parseColourIndexOC,		/* 24 line colour index */
    parseColourOC,		/* 25 line colour */
    parseSetAttribute,		/* 26 curve approximation method */
    parseSetAttribute,		/* 27 polyline interpolation method */
    parseSetAttribute,		/* 28 line bundle index */
    parseSetAttribute,		/* 29 surface interior style */
    parseSetAttribute,		/* 30 surface interior style index */
    parseColourIndexOC,		/* 31 surface colour index */
    parseColourOC,		/* 32 surface colour */
    parseSetAttribute,		/* 33 surface reflection attributes */
    parseSetAttribute,		/* 34 surface reflection model */
    parseSetAttribute,		/* 35 surface interpolation method */
    parseSetAttribute,		/* 36 backface surface interior style */
    parseSetAttribute,		/* 37 backface surface interior style index */
    parseColourIndexOC,		/* 38 backface surface colour index */
    parseColourOC,		/* 39 backface surface colour */
    parseSetAttribute,		/* 40 backface surface reflection attributes */
    parseSetAttribute,		/* 41 backface surface reflection model */
    parseSetAttribute,		/* 42 backface surface interpolation method */
    parseSetAttribute,		/* 43 surface approximation */
    parseSetAttribute,		/* 44 facet culling mode */
    parseSetAttribute,		/* 45 facet distinguish flag */
    parseSetAttribute,		/* 46 pattern size */
    parseSetAttribute,		/* 47 pattern reference point */
    parseSetAttribute,		/* 48 pattern reference point and vectors */
    parseSetAttribute,		/* 49 interior bundle index */
    parseSetAttribute,		/* 50 surface edge flag */
    parseSetAttribute,		/* 51 surface edge type */
    parseSetAttribute,		/* 52 surface edge width */
    parseColourIndexOC,		/* 53 surface edge colour index */
    parseColourOC,		/* 54 surface edge colour */
    parseSetAttribute,		/* 55 edge bundle index */
    parseSetAttribute,		/* 56 set individual asf */
    parseSetAttribute,		/* 57 local transform 3d */
    parseSetAttribute,		/* 58 local transform 2d */
    parseSetAttribute,		/* 59 global transform 3d */
    parseSetAttribute,		/* 60 global transform 2d */
    parseSetAttribute,		/* 61 model clip */
    parseSetMCVolume,		/* 62 set model clip volume 3d */
    parseSetMCVolume2D,		/* 63 set model clip volume 2d */
    parseSetAttribute,		/* 64 restore model clip volume */
    parseSetAttribute,		/* 65 view index */
    replaceLightState,		/* 66 light source state */
    parseSetAttribute,		/* 67 depth cue index */
    parseSetAttribute,		/* 68 pick id */
    parseSetAttribute,		/* 69 hlhsr identifier */
    parseSetAttribute,		/* 70 colour approx index */
    parseSetAttribute,		/* 71 rendering colour model */
    parsePSurfaceChars,		/* 72 parametric surface attributes */
    parseSetAttribute,		/* 73 add names to name set */
    parseSetAttribute,		/* 74 remove names from name set */
    parseSetAttribute,		/* 75 execute structure */
    parseSetAttribute,		/* 76 label */
    parseSetAttribute,		/* 77 application data */
    parseSetAttribute,		/* 78 gse */
    parseMarker,		/* 79 marker 3d */
    parseMarker2D,		/* 80 marker 2d */
    parseText,			/* 81 text3d */
    parseText2D,		/* 82 text2d */
    parseAnnotationText,	/* 83 annotation text3d */
    parseAnnotationText2D,	/* 84 annotation text2d */
    parsePolyline,		/* 85 polyline3d */
    parsePolyline2D,		/* 86 polyline2d */
    parsePolylineSet,		/* 87 polyline set 3d with data */
    parseNurbCurve,		/* 88 non-uniform b spline curve */
    parseFillArea,		/* 89 fill area 3d */
    parseFillArea2D,		/* 90 fill area 2d */
    parseExtFillArea,		/* 91 fill area 3d with data */
    parseFillAreaSet,		/* 92 fill area set 3d */
    parseFillAreaSet2D,		/* 93 fill area set 2d */
    parseExtFillAreaSet,	/* 94 fill area set 3d with data */
    parseTriangleStrip,		/* 95 triangle strip */
    parseQuadrilateralMesh,	/* 96 quadrilateral mesh */
    replaceSOFAS,		/* 97 set of fill area sets */
    replaceNurbSurface,		/* 98 non-uniform b spline surface */
    parseCellArray,		/* 99 cell array 3d */
    parseCellArray2D,		/* 100 cell array 2d */
    parseExtCellArray,		/* 101 extended cell array 3d */
    parseGdp,			/* 102 gdp 3d */
    parseGdp2D,			/* 103 gdp 2d */
    parseSetAttribute		/* 104 Noop */
};
