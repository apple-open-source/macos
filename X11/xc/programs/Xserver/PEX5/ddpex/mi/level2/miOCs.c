/* $Xorg: miOCs.c,v 1.6 2001/02/09 02:04:10 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miOCs.c,v 3.7 2001/12/14 19:57:29 dawes Exp $ */

#include "miLUT.h"
#include "misc.h"
#include "miscstruct.h"
#include "PEXErr.h"
#include "PEXprotost.h"
#include "ddpex2.h"
#include "miRender.h"
#include "miStruct.h"
#include "gcstruct.h"
#include "miLight.h"
#include "pexos.h"


/* Level II Output Command Attributes */

/*
 * Marker type
 */
ddpex2rtn
miMarkerType(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexMarkerType *pMT = (pexMarkerType *)(pOC+1);

    pddc->Dynamic->pPCAttr->markerType = pMT->markerType;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXMarkerTypeAsf) != PEXBundled)
	pddc->Static.attrs->markerType = pMT->markerType;

    return(Success);
}

/*
 * Marker scale
 */
ddpex2rtn
miMarkerScale(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexMarkerScale *pMS = (pexMarkerScale *)(pOC+1);

    pddc->Dynamic->pPCAttr->markerScale = pMS->scale;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXMarkerScaleAsf) != PEXBundled)
	pddc->Static.attrs->markerScale = pMS->scale;

    return(Success);
}

/*
 * Marker Bundle Index
 */
ddpex2rtn
miMarkerBundleIndex(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexMarkerBundleIndex *pMBI = (pexMarkerBundleIndex *)(pOC+1);
    ddBitmask    tables, namesets, attrs;

    if (pddc->Dynamic->pPCAttr->markerIndex != pMBI->index) {
	pddc->Dynamic->pPCAttr->markerIndex = pMBI->index;

	namesets = attrs = 0;
	tables = PEXDynMarkerBundle;
	ValidateDDContextAttrs(pRend, pddc, tables, namesets, attrs);
    }
    return(Success);
}

/*
 * Text Font Index
 */
ddpex2rtn
miTextFontIndex(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexTextFontIndex *pMBI = (pexTextFontIndex *)(pOC+1);
    pddc->Dynamic->pPCAttr->textFont = pMBI->index;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXTextFontIndexAsf) != PEXBundled)
	pddc->Static.attrs->textFont = pMBI->index;
    return(Success);
}

/*
 * Text Precision
 */
ddpex2rtn
miTextPrecision(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexTextPrecision *pMBI = (pexTextPrecision *)(pOC+1);
    pddc->Dynamic->pPCAttr->textPrecision = pMBI->precision;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXTextPrecAsf) != PEXBundled)
	pddc->Static.attrs->textPrecision = pMBI->precision;
    return(Success);
}

/*
 * Character Expansion
 */
ddpex2rtn
miCharExpansion(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexCharExpansion *pMBI = (pexCharExpansion *)(pOC+1);
    pddc->Dynamic->pPCAttr->charExpansion = pMBI->expansion;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXCharExpansionAsf) != PEXBundled)
	pddc->Static.attrs->charExpansion = pMBI->expansion;
    return(Success);
}

/*
 * Character Spacing
 */
ddpex2rtn
miCharSpacing(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexCharSpacing *pMBI = (pexCharSpacing *)(pOC+1);
    pddc->Dynamic->pPCAttr->charSpacing = pMBI->spacing;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXCharSpacingAsf) != PEXBundled)
	pddc->Static.attrs->charSpacing = pMBI->spacing;
    return(Success);
}

/*
 * Character Height
 */
ddpex2rtn
miCharHeight(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexCharHeight *pMBI = (pexCharHeight *)(pOC+1);
    pddc->Dynamic->pPCAttr->charHeight = pMBI->height;
    pddc->Static.attrs->charHeight = pMBI->height;
    return(Success);
}

/*
 * Character Up Vector
 */
ddpex2rtn
miCharUpVector(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexCharUpVector *pMBI = (pexCharUpVector *)(pOC+1);
    pddc->Dynamic->pPCAttr->charUp.x = pMBI->up.x;
    pddc->Dynamic->pPCAttr->charUp.y = pMBI->up.y;
    pddc->Static.attrs->charUp = pddc->Dynamic->pPCAttr->charUp;
    return(Success);
}

/*
 * Text Path
 */
ddpex2rtn
miTextPath(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexTextPath *pMBI = (pexTextPath *)(pOC+1);
    pddc->Dynamic->pPCAttr->textPath = pMBI->path;
    pddc->Static.attrs->textPath = pMBI->path;
    return(Success);
}

/*
 * Text Alignment
 */
ddpex2rtn
miTextAlignment(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexTextAlignment *pMBI = (pexTextAlignment *)(pOC+1);
    pddc->Dynamic->pPCAttr->textAlignment.vertical =
					    (ddUSHORT) pMBI->alignment.vertical;
     pddc->Dynamic->pPCAttr->textAlignment.horizontal =
					   (ddUSHORT) pMBI->alignment.horizontal;
     pddc->Static.attrs->textAlignment = 
				    pddc->Dynamic->pPCAttr->textAlignment;

    return(Success);
}

/*
 * Annotation Text Height
 */
ddpex2rtn
miAtextHeight(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexAtextHeight *pMBI = (pexAtextHeight *)(pOC+1);
    pddc->Dynamic->pPCAttr->atextHeight = pMBI->height;
    pddc->Static.attrs->atextHeight = pMBI->height;
    return(Success);
}

/*
 * Annotation Text Up Vector
 */
ddpex2rtn
miAtextUpVector(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexAtextUpVector *pMBI = (pexAtextUpVector *)(pOC+1);
    pddc->Dynamic->pPCAttr->atextUp.x = pMBI->up.x;
    pddc->Dynamic->pPCAttr->atextUp.y = pMBI->up.y;
    pddc->Static.attrs->atextUp = pddc->Dynamic->pPCAttr->atextUp;
    return(Success);
}

/*
 * Annotation Text Path
 */
ddpex2rtn
miAtextPath(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexAtextPath *pMBI = (pexAtextPath *)(pOC+1);
    pddc->Dynamic->pPCAttr->atextPath = pMBI->path;
    pddc->Static.attrs->atextPath = pMBI->path;
    return(Success);
}

/*
 * Annotation Text Alignment
 */
ddpex2rtn
miAtextAlignment(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexAtextAlignment *pMBI = (pexAtextAlignment *)(pOC+1);
    pddc->Dynamic->pPCAttr->atextAlignment.vertical =
					    (ddUSHORT) pMBI->alignment.vertical;
    pddc->Dynamic->pPCAttr->atextAlignment.horizontal =
					   (ddUSHORT) pMBI->alignment.horizontal;
    pddc->Static.attrs->atextAlignment = 
					pddc->Dynamic->pPCAttr->atextAlignment;
    return(Success);
}

/*
 * Annotation Text Style
 */
ddpex2rtn
miAtextStyle(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexAtextStyle *pMT = (pexAtextStyle *)(pOC+1);
    pddc->Dynamic->pPCAttr->atextStyle = pMT->style;
    pddc->Static.attrs->atextStyle = pMT->style;
    return(Success);
}

/*
 * Text Bundle Index
 */
ddpex2rtn
miTextBundleIndex(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexTextBundleIndex *pTBI = (pexTextBundleIndex *)(pOC+1);
    ddBitmask    tables, namesets, attrs;

    if (pddc->Dynamic->pPCAttr->textIndex != pTBI->index) {
	pddc->Dynamic->pPCAttr->textIndex = pTBI->index;

	namesets = attrs = 0;
	tables = PEXDynTextBundle;
	ValidateDDContextAttrs(pRend, pddc, tables, namesets, attrs);
    }
    return(Success);
}

/*
 * Line type (Dashing style)
 */
ddpex2rtn
miLineType(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexLineType *pLT = (pexLineType *)(pOC+1);
    pddc->Dynamic->pPCAttr->lineType = pLT->lineType;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXLineTypeAsf) != PEXBundled) {
	pddc->Static.attrs->lineType = pLT->lineType;
	pddc->Static.misc.flags |= POLYLINEGCFLAG;
    }
    return(Success);
}


/*
 * Line Width 
 */
ddpex2rtn
miLineWidth(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexLineWidth *pLW = (pexLineWidth *)(pOC+1);
    pddc->Dynamic->pPCAttr->lineWidth = pLW->width;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXLineWidthAsf) != PEXBundled) {
	pddc->Static.attrs->lineWidth = pLW->width;
	pddc->Static.misc.flags |= POLYLINEGCFLAG;
    }
    return(Success);
}

/*
 * Line Bundle Index
 */
ddpex2rtn
miLineBundleIndex(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexLineBundleIndex *pLBI = (pexLineBundleIndex *)(pOC+1);
    ddBitmask    tables, namesets, attrs;

    if (pddc->Dynamic->pPCAttr->lineIndex != pLBI->index) {
	pddc->Dynamic->pPCAttr->lineIndex = pLBI->index;

	namesets = attrs = 0;
	tables = PEXDynLineBundle;
	ValidateDDContextAttrs(pRend, pddc, tables, namesets, attrs);
    }
    return(Success);
}

/*
 * curve approximation method
 */
ddpex2rtn
miCurveApproximation(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexCurveApproximation *pCA = (pexCurveApproximation *)(pOC+1);
    pddc->Dynamic->pPCAttr->curveApprox.approxMethod = pCA->approx.approxMethod;
    pddc->Dynamic->pPCAttr->curveApprox.tolerance = pCA->approx.tolerance;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXLineWidthAsf) != PEXBundled)
	pddc->Static.attrs->curveApprox=pddc->Dynamic->pPCAttr->curveApprox;

    return(Success);
}


/*
 * Surface interior style
 */
ddpex2rtn
miInteriorStyle(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexInteriorStyle *pIS = (pexInteriorStyle *)(pOC+1);
    pddc->Dynamic->pPCAttr->intStyle = pIS->interiorStyle;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXInteriorStyleAsf) != PEXBundled) {
	pddc->Static.attrs->intStyle = pIS->interiorStyle;
	pddc->Static.misc.flags |= FILLAREAGCFLAG;
    }
    return(Success);
}

/*
 * Depth Cue Bundle Index
 */
ddpex2rtn
miDepthCueIndex(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexDepthCueIndex *pDCI = (pexDepthCueIndex *)(pOC+1);
    ddBitmask    tables, namesets, attrs;

    pddc->Dynamic->pPCAttr->depthCueIndex = pDCI->index;

    /* Mark as invalid cc version of depth cue entry in dd context */
    pddc->Static.misc.flags |= CC_DCUEVERSION;

    namesets = attrs = 0;
    tables = PEXDynDepthCueTableContents;
    ValidateDDContextAttrs(pRend, pddc, tables, namesets, attrs);

    return(Success);
}

/*
 * Interior Bundle Index
 */
ddpex2rtn
miInteriorBundleIndex(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexInteriorBundleIndex *pIBI = (pexInteriorBundleIndex *)(pOC+1);
    ddBitmask    tables, namesets, attrs;

    if (pddc->Dynamic->pPCAttr->intIndex != pIBI->index) {
	pddc->Dynamic->pPCAttr->intIndex = pIBI->index;

	namesets = attrs = 0;
	tables = PEXDynInteriorBundle;
	ValidateDDContextAttrs(pRend, pddc, tables, namesets, attrs);
    }
    return(Success);
}

/*
 * Surface reflection attributes
 */
ddpex2rtn
miSurfaceReflAttr(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexSurfaceReflAttr *pSRA = (pexSurfaceReflAttr *)(pOC+1);
    ddColourSpecifier *pSC = 
		    (ddColourSpecifier *)&pSRA->reflectionAttr.specularColour;

    pddc->Dynamic->pPCAttr->reflAttr.ambient =
					(ddFLOAT)pSRA->reflectionAttr.ambient;
    pddc->Dynamic->pPCAttr->reflAttr.diffuse =
					(ddFLOAT)pSRA->reflectionAttr.diffuse;
    pddc->Dynamic->pPCAttr->reflAttr.specular =
					(ddFLOAT)pSRA->reflectionAttr.specular;
    pddc->Dynamic->pPCAttr->reflAttr.specularConc =
				    (ddFLOAT)pSRA->reflectionAttr.specularConc;
    pddc->Dynamic->pPCAttr->reflAttr.transmission =
				    (ddFLOAT)pSRA->reflectionAttr.transmission;

    switch (pddc->Dynamic->pPCAttr->reflAttr.specularColour.colourType = 
		pSC->colourType) {
	case PEXIndexedColour:
	    pddc->Dynamic->pPCAttr->reflAttr.specularColour.colour.indexed = 
							    pSC->colour.indexed;
	    break;
	case PEXRgbFloatColour:
	case PEXCieFloatColour:
	case PEXHsvFloatColour:
	case PEXHlsFloatColour:
	    pddc->Dynamic->pPCAttr->reflAttr.specularColour.colour.rgbFloat = 
							    pSC->colour.rgbFloat;
	    break;
	case PEXRgb8Colour:
	    pddc->Dynamic->pPCAttr->reflAttr.specularColour.colour.rgb8 = 
								pSC->colour.rgb8;
	    break;
	case PEXRgb16Colour:
	    pddc->Dynamic->pPCAttr->reflAttr.specularColour.colour.rgb16 = 
							    pSC->colour.rgb16;
	    break;
    }

    /* Update DDC rendering attributes if not bundled */
    if((pddc->Dynamic->pPCAttr->asfs & PEXReflectionAttrAsf) != PEXBundled)
       pddc->Static.attrs->reflAttr = pddc->Dynamic->pPCAttr->reflAttr;

    return(Success);
}
/*
 * Surface reflection model
 */
ddpex2rtn
miSurfaceReflModel(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexSurfaceReflModel *pSRM = (pexSurfaceReflModel *)(pOC+1);
    pddc->Dynamic->pPCAttr->reflModel = pSRM->reflectionModel;
    /* Update DDC rendering attributes if not bundled */
    if((pddc->Dynamic->pPCAttr->asfs & PEXReflectionModelAsf)!=PEXBundled)
	pddc->Static.attrs->reflModel = pSRM->reflectionModel;
    return(Success);
}

/*
 * Surface interpolation scheme
 */
ddpex2rtn
miSurfaceInterp(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexSurfaceInterp *pSI = (pexSurfaceInterp *)(pOC+1);
    pddc->Dynamic->pPCAttr->surfInterp = pSI->surfaceInterp;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceInterpAsf) != PEXBundled)
	pddc->Static.attrs->surfInterp = pSI->surfaceInterp;
    return(Success);
}

/* 
 * Surface approximation criteria
 */
ddpex2rtn
miSurfaceApproximation(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexSurfaceApproximation *pSA = (pexSurfaceApproximation *)(pOC+1);
    pddc->Dynamic->pPCAttr->surfApprox.approxMethod = pSA->approx.approxMethod;
    pddc->Dynamic->pPCAttr->surfApprox.uTolerance = pSA->approx.uTolerance;
    pddc->Dynamic->pPCAttr->surfApprox.vTolerance = pSA->approx.vTolerance;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceApproxAsf) != PEXBundled)
	pddc->Static.attrs->surfApprox = pddc->Dynamic->pPCAttr->surfApprox;
    return(Success);
}

/* 
 * Cull back or front facing facets.
 */
ddpex2rtn
miCullingMode(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexCullingMode *pCM = (pexCullingMode *)(pOC+1);
    pddc->Dynamic->pPCAttr->cullMode = pCM->cullMode;
    return(Success);
}

/*
 * Surface edge flag (enable/disable dispaly of fill area edges)
 */
ddpex2rtn
miSurfaceEdgeFlag(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexSurfaceEdgeFlag *pSEF = (pexSurfaceEdgeFlag *)(pOC+1);
    pddc->Dynamic->pPCAttr->edges = pSEF->onoff;
    /*set the dd context edge visibility flag if not a bundled attribute*/
    if((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceEdgesAsf) != PEXBundled) {
	pddc->Static.attrs->edges = pSEF->onoff;
	pddc->Static.misc.flags |= EDGEGCFLAG;
    }
    return(Success);
}
/*
 * Surface edge type (Dashing style)
 */
ddpex2rtn
miSurfaceEdgeType(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexSurfaceEdgeType *pSET = (pexSurfaceEdgeType *)(pOC+1);
    pddc->Dynamic->pPCAttr->edgeType = pSET->edgeType;
    /* Update DDC rendering attributes if not bundled */
    if ((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceEdgeTypeAsf)!=PEXBundled) {
	pddc->Static.attrs->edgeType = pSET->edgeType;
	pddc->Static.misc.flags |= EDGEGCFLAG;
    }
    return(Success);
}


/*
 * Surface edge Width 
 */
ddpex2rtn
miSurfaceEdgeWidth(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexSurfaceEdgeWidth *pSEW = (pexSurfaceEdgeWidth *)(pOC+1);
    pddc->Dynamic->pPCAttr->edgeWidth = pSEW->width;
    /* Update DDC rendering attributes if not bundled */
    if((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceEdgeWidthAsf)!=PEXBundled) {
	pddc->Static.attrs->edgeWidth = pSEW->width;
	pddc->Static.misc.flags |= EDGEGCFLAG;
    }
    return(Success);
}
/*
 * Surface edge Bundle Index
 */
ddpex2rtn
miEdgeBundleIndex(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexEdgeBundleIndex    *pEBI = (pexEdgeBundleIndex *)(pOC+1);
    ddBitmask    tables, namesets, attrs;

    if (pddc->Dynamic->pPCAttr->edgeIndex != pEBI->index) {
	pddc->Dynamic->pPCAttr->edgeIndex = pEBI->index;

	namesets = attrs = 0;
	tables = PEXDynEdgeBundle;
	ValidateDDContextAttrs(pRend, pddc, tables, namesets, attrs);
    }
    return(Success);
}

/*
 * Set ASF values
 */
ddpex2rtn
miSetAsfValues(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexSetAsfValues    *pSAV = (pexSetAsfValues *)(pOC+1);
    ddBitmask	tables, namesets, attrs;

    if (pSAV->source == PEXBundled) { /* Note that PEXBundled == 0 */
	pddc->Dynamic->pPCAttr->asfs &= ~(pSAV->attribute);

    } else {
	pddc->Dynamic->pPCAttr->asfs |= pSAV->attribute;

    }

    /* changing the table behaves the same as changing 
     * the asf, so use that for ValidateDDContext
     */
    tables = namesets = attrs = 0;
    if (pSAV->attribute & ( PEXMarkerTypeAsf | 
			    PEXMarkerScaleAsf | 
			    PEXMarkerColourAsf))
	tables |= PEXDynMarkerBundle;
     if (pSAV->attribute & (PEXTextFontIndexAsf | 
			    PEXTextPrecAsf | 
			    PEXCharExpansionAsf | 
			    PEXCharSpacingAsf | 
			    PEXTextColourAsf))
	tables |= PEXDynTextBundle;
     if (pSAV->attribute & (PEXLineTypeAsf | 
			    PEXLineWidthAsf | 
			    PEXLineColourAsf |
			    PEXCurveApproxAsf |
			    PEXPolylineInterpAsf))
	tables |= PEXDynLineBundle;
     if (pSAV->attribute & (PEXInteriorStyleAsf |
			    PEXInteriorStyleIndexAsf |
			    PEXSurfaceColourAsf |
			    PEXSurfaceInterpAsf |
			    PEXReflectionModelAsf |
			    PEXReflectionAttrAsf |
			    PEXBfInteriorStyleAsf |
			    PEXBfInteriorStyleIndexAsf |
			    PEXBfSurfaceColourAsf |
			    PEXBfSurfaceInterpAsf |
			    PEXBfReflectionModelAsf |
			    PEXBfReflectionAttrAsf |
			    PEXSurfaceApproxAsf))
	tables |= PEXDynInteriorBundle;
    if (pSAV->attribute & ( PEXSurfaceEdgeTypeAsf | 
			    PEXSurfaceEdgeWidthAsf | 
			    PEXSurfaceEdgeColourAsf |
			    PEXSurfaceEdgesAsf ))
	tables |= PEXDynEdgeBundle;

    /* Re-initialize the dd context rendering attributes */
    ValidateDDContextAttrs(pRend, pddc, tables, namesets, attrs);

    return(Success);
}

/*
 * Local transformations
 */
ddpex2rtn
miLocalTransform(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexLocalTransform *pLT = (pexLocalTransform *)(pOC+1);

    switch (pLT->compType) {
	case PEXPreConcatenate:
	    miMatMult(	pddc->Dynamic->pPCAttr->localMat,
			pLT->matrix, pddc->Dynamic->pPCAttr->localMat);
	    break;
	case PEXPostConcatenate:
	    miMatMult(	pddc->Dynamic->pPCAttr->localMat,
			pddc->Dynamic->pPCAttr->localMat, pLT->matrix);
	    break;
	case PEXReplace:
	    memcpy( (char *)(pddc->Dynamic->pPCAttr->localMat),
		   (char *)(pLT->matrix),
                   16*sizeof(ddFLOAT));
	    break;
    }

    /* Update composite transforms */
    /* First, composite [CMM] */
    miMatMult(	pddc->Dynamic->mc_to_wc_xform,
		pddc->Dynamic->pPCAttr->localMat,
		pddc->Dynamic->pPCAttr->globalMat);

    /* Next, composite [VCM] next */
     miMatMult(	pddc->Dynamic->mc_to_cc_xform,
		pddc->Dynamic->mc_to_wc_xform, 
		pddc->Dynamic->wc_to_cc_xform);

    /* Lastly, Compute the composite mc -> dc transform */
    miMatMult(	pddc->Dynamic->mc_to_dc_xform,
		pddc->Dynamic->mc_to_cc_xform,
		pddc->Dynamic->cc_to_dc_xform);

    /* Mark as invalid appropriate inverse transforms in dd context */
    pddc->Static.misc.flags |= (INVTRMCTOWCXFRMFLAG | INVTRWCTOCCXFRMFLAG);

    return(Success);
}

ddpex2rtn
miLocalTransform2D(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexLocalTransform2D *pLT = (pexLocalTransform2D *)(pOC+1);
    ddFLOAT	     *s, *d;
    ddFLOAT	     temp[4][4];

    /*
     * 2D -> 3D transform as follows:
     *
     *    a d g	 a d 0 g
     *    b e h	 b e 0 h
     *    c f i	 0 0 1 0
     *		 c f 0 i
     *
     */
    s = (ddFLOAT *)pLT->matrix3X3;
    d = (ddFLOAT *)temp;

    *d++ = *s++;    /* [0][0] */ 
    *d++ = *s++;    /* [0][1] */ 
    *d++ = 0.0;    /* [0][2] */
    *d++ = *s++;    /* [0][3] */
    *d++ = *s++;    /* [1][0] */
    *d++ = *s++;    /* [1][1] */
    *d++ = 0.0;    /* [1][2] */
    *d++ = *s++;    /* [1][3] */
    *d++ = 0.0;    /* [2][0] */
    *d++ = 0.0;    /* [2][1] */
    *d++ = 1.0;    /* [2][2] */
    *d++ = 0.0;    /* [2][3] */
    *d++ = *s++;    /* [3][0] */
    *d++ = *s++;    /* [3][0] */
    *d++ = 0.0;    /* [3][0] */
    *d++ = *s++;    /* [3][0] */

    switch (pLT->compType) {
	case PEXPreConcatenate:
	    miMatMult(	pddc->Dynamic->pPCAttr->localMat,
			temp, pddc->Dynamic->pPCAttr->localMat);
	    break;
	case PEXPostConcatenate:
	    miMatMult(	pddc->Dynamic->pPCAttr->localMat,
			pddc->Dynamic->pPCAttr->localMat, temp);
	    break;
	case PEXReplace:
	    memcpy( (char *)(pddc->Dynamic->pPCAttr->localMat), (char *)temp,  
		    16*sizeof(ddFLOAT));
	    break;
    }

    /* Update composite transforms */
    /* First, composite [CMM] */
    miMatMult(	pddc->Dynamic->mc_to_wc_xform,
		pddc->Dynamic->pPCAttr->localMat,
		pddc->Dynamic->pPCAttr->globalMat);

    /* Next, composite [VCM] next */
    miMatMult(	pddc->Dynamic->mc_to_cc_xform,
		pddc->Dynamic->mc_to_wc_xform, 
		pddc->Dynamic->wc_to_cc_xform);

    /* Lastly, Compute the composite mc -> dc transform */
    miMatMult(	pddc->Dynamic->mc_to_dc_xform,
		pddc->Dynamic->mc_to_cc_xform,
		pddc->Dynamic->cc_to_dc_xform);

    /* Mark as invalid appropriate inverse transforms in dd context */
    pddc->Static.misc.flags |= (INVTRMCTOWCXFRMFLAG | INVTRWCTOCCXFRMFLAG);

    return(Success);
}


/*
 * Global transformations
 */
ddpex2rtn
miGlobalTransform(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexGlobalTransform *pGT = (pexGlobalTransform *)(pOC+1);

    memcpy( (char *)(pddc->Dynamic->pPCAttr->globalMat), (char *)(pGT->matrix), 
	    16 * sizeof(ddFLOAT));

    /* Update composite transforms */
    /* First, composite [CMM] */
    miMatMult(	pddc->Dynamic->mc_to_wc_xform,
		pddc->Dynamic->pPCAttr->localMat,
		pddc->Dynamic->pPCAttr->globalMat);

    /* Next, composite [VCM] next */
    miMatMult(	pddc->Dynamic->mc_to_cc_xform,
		pddc->Dynamic->mc_to_wc_xform, 
		pddc->Dynamic->wc_to_cc_xform);

    /* Lastly, Compute the composite mc -> dc transform */
    miMatMult(	pddc->Dynamic->mc_to_dc_xform,
		pddc->Dynamic->mc_to_cc_xform,
		pddc->Dynamic->cc_to_dc_xform);
     
    /* Mark as invalid appropriate inverse transforms in dd context */
    pddc->Static.misc.flags |= (INVTRMCTOWCXFRMFLAG | INVTRWCTOCCXFRMFLAG);

    return(Success);
}

ddpex2rtn
miGlobalTransform2D(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexGlobalTransform2D *pGT = (pexGlobalTransform2D *)(pOC+1);
    ddFLOAT	      *s, *d;

    /*
     * 2D -> 3D transform as follows:
     *
     *    a d g	 a d 0 g
     *    b e h	 b e 0 h
     *    c f i	 0 0 1 0
     *		 c f 0 i
     *
     */
    s = (ddFLOAT *)pGT->matrix3X3;
    d = (ddFLOAT *)pddc->Dynamic->pPCAttr->globalMat;

    *d++ = *s++;    /* [0][0] */
    *d++ = *s++;    /* [0][1] */
    *d++ = 0.0;    /* [0][2] */
    *d++ = *s++;    /* [0][3] */
    *d++ = *s++;    /* [1][0] */
    *d++ = *s++;    /* [1][1] */
    *d++ = 0.0;    /* [1][2] */
    *d++ = *s++;    /* [1][3] */
    *d++ = 0.0;    /* [2][0] */
    *d++ = 0.0;    /* [2][1] */
    *d++ = 1.0;    /* [2][2] */
    *d++ = 0.0;    /* [2][3] */
    *d++ = *s++;    /* [3][0] */
    *d++ = *s++;    /* [3][0] */
    *d++ = 0.0;    /* [3][0] */
    *d++ = *s++;    /* [3][0] */

    /* Update composite transforms */
    /* First, composite [CMM] */
    miMatMult(	pddc->Dynamic->mc_to_wc_xform,
		pddc->Dynamic->pPCAttr->localMat,
		pddc->Dynamic->pPCAttr->globalMat);

    /* Next, composite [VCM] next */
    miMatMult(	pddc->Dynamic->mc_to_cc_xform,
		pddc->Dynamic->mc_to_wc_xform, 
		pddc->Dynamic->wc_to_cc_xform);

   /* Lastly, Compute the composite mc -> dc transform */
    miMatMult(	pddc->Dynamic->mc_to_dc_xform,
		pddc->Dynamic->mc_to_cc_xform,
		pddc->Dynamic->cc_to_dc_xform);

    /* Mark as invalid appropriate inverse transforms in dd context */
    pddc->Static.misc.flags |= (INVTRMCTOWCXFRMFLAG | INVTRWCTOCCXFRMFLAG);

    return(Success);
}

ddpex2rtn
miModelClip(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexModelClip *pMC = (pexModelClip *)(pOC+1);
    pddc->Dynamic->pPCAttr->modelClip = pMC->onoff;

    return(Success);
}



ddpex2rtn
miViewIndex(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexViewIndex    *pVI = (pexViewIndex *)(pOC+1);

    /* first, make sure this is a new index */
    if (pddc->Dynamic->pPCAttr->viewIndex == pVI->index) return(Success);

    /* Copy new index into ddContext */
    pddc->Dynamic->pPCAttr->viewIndex = pVI->index;

    /* Now, update internal transform cache to reflect new index */
    miBldCC_xform(pRend, pddc);

    return(Success);
}



ddpex2rtn
miPickId(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexPickId    *pPI = (pexPickId *)(pOC+1);

    pddc->Dynamic->pPCAttr->pickId = pPI->pickId;
    return(Success);
}


ddpex2rtn
miColourApproxIndex(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexColourApproxIndex    *pCAI = (pexColourApproxIndex *)(pOC+1);
    pddc->Dynamic->pPCAttr->colourApproxIndex = pCAI->index;
    return(Success);
}


ddpex2rtn
miRenderingColourModel(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexRenderingColourModel    *pRCM = (pexRenderingColourModel *)(pOC+1);
    pddc->Dynamic->pPCAttr->rdrColourModel = pRCM->model;
    return(Success);
}


ddpex2rtn
miParaSurfCharacteristics(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    miPSurfaceCharsStruct *pPSC = (miPSurfaceCharsStruct *)(pOC+1);
    switch (pddc->Dynamic->pPCAttr->psc.type = pPSC->type) {
	case PEXPSCNone:
	case PEXPSCImpDep:
	    break;
	case PEXPSCIsoCurves:
	    pddc->Dynamic->pPCAttr->psc.data.isoCurves =  *pPSC->data.pIsoCurves;
	    break;
	case PEXPSCMcLevelCurves:
	    /* Note that level curves are not implemented */
	    pddc->Dynamic->pPCAttr->psc.data.mcLevelCurves = 
						    *pPSC->data.pMcLevelCurves;
	    break;
	case PEXPSCWcLevelCurves:
	    /* Note that level curves are not implemented */
	    pddc->Dynamic->pPCAttr->psc.data.wcLevelCurves = 
						    *pPSC->data.pWcLevelCurves;
	    break;
	default:
	    break;
    }
    return(Success);

}

ddpex2rtn
miAddToNameSet(pRend, pOC) /* and RemoveNameFromNameSet */
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    miDDContext    *pddc = (miDDContext *)(pRend->pDDContext);
    pexAddToNameSet *pANS = (pexAddToNameSet *)(pOC+1);
    ddULONG    *pName = (ddULONG *)(pANS + 1);
    register int num = (pANS->head.length) - 1;
    ddUSHORT    save_flags = pddc->Dynamic->filter_flags;
    ddBitmask   namesets;
    extern void    ValidateFilters();

    for (; num>0; num--, pName++)
	/* ignore values that are out of range */
	if ( MINS_VALID_NAME(*pName) ) {
	    if (pANS->head.elementType == PEXOCAddToNameSet) {
		MINS_ADD_TO_NAMESET(*pName, pddc->Dynamic->currentNames);
	    } else {
		MINS_REMOVE_FROM_NAMESET(*pName, pddc->Dynamic->currentNames);
	    }
	}	

    /* changing current namesset, so update all filters */
    namesets = PEXDynHighlightNameset |
               PEXDynInvisibilityNameset |
               PEXDynHighlightNamesetContents |
               PEXDynInvisibilityNamesetContents;


    ValidateFilters(pRend, pddc, namesets);

    return(Success);
}


ddpex2rtn
miExecuteStructure(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    extern void    execute_structure_OC();

    execute_structure_OC(pRend, (pexExecuteStructure *)(pOC+1));
    return(Success);
}


ddpex2rtn
miNoop(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
    return (Success);
}


ddpex2rtn
miUndefined(pRend, pOC)
    ddRendererPtr    pRend;
    miGenericStr    *pOC;
{
#ifdef PR_INFO
    ErrorF( "Attribute is not implemented\n");
#endif
    return (PEXNYI);
}



/*++
 |
 |  Function Name:    miLightStateOC
 |
 |  Function Description:
 |     Handles PEXOCLightState OC
 |
 |  Note(s):
 |
 --*/
ddpex2rtn
miLightStateOC(pRend, pOC)
/* in */
    ddRendererPtr       pRend;      /* renderer handle */
    miGenericStr       *pOC;      /* output command */
/* out */
{
    miDDContext		*pddc = (miDDContext *)(pRend->pDDContext);
    miLightStateStruct	*pLS = (miLightStateStruct *)(pOC+1);
    ddUSHORT		*ptr;
    int			i;

    /*
     * Merge in a new set of light source indices
     */
    if (pLS->enableList->numObj > 0)
	puMergeLists(	pddc->Dynamic->pPCAttr->lightState, pLS->enableList,
			pddc->Dynamic->pPCAttr->lightState);

    if (pLS->disableList->numObj > 0)
	for (i = pLS->disableList->numObj, 
	    ptr = (ddUSHORT *)pLS->disableList->pList;
	    i > 0; 
	    --i, ptr++)
	    puRemoveFromList(ptr, pddc->Dynamic->pPCAttr->lightState);

    return (Success);
}
 
/*++
 | 
 |  Function Name:     SetMCVolume 
 |
 |  Function Description:
 |       Handles Handles SetModelClipVolume and 
 |     SetModelClipVolume2D OCs
 |
 |  Note(s):    Although model clipping half spaces are	specified 
 |	in modelling space, the pipeline stores the current clipping 
 |	volume in world coordinates.  This is because the modelling
 |	transformation matrix can change independently of this particular 
 |	OC, and world coordinates are the the only common coordinates to 
 |	intersect.  HOWEVER, when model clipping is enabled, this
 |	intersect volume is transferred BACK to model space and stored 
 |	in pddc->Static->ms_MCV for the actual clipping. This is done 
 |	in order to "prune" out as many primitives as early as possible
 |	in the pipeline. 
 |
 |	Whenever the modelling transformation matrix is changed, or
 |	when a new renderer is created, the model space	version of 
 |	the model clipping volume is invalidated; the world 
 |	coordinate volume is the "reference." The validation flag is 
 |	the INVMCXFRMFLAG defined in ddpex/mi/include/miRender.h
 |
 --*/
ddpex2rtn
miSetMCVolume(pRend, pOC)
/* in */
    ddRendererPtr       pRend;    /* renderer handle */
    miGenericStr       *pOC;      /* output command */
/* out */
{
    miDDContext		*pddc = (miDDContext *)(pRend->pDDContext);
    miMCVolume_Struct	*OC_MCV = (miMCVolume_Struct *)(pOC+1);
    listofObj		*pc_MCV;
    ddHalfSpace		*OC_HS, tmp_HS;
    int			i, count;
    ddFLOAT		length;

    static  ddFLOAT	vect_xform[4][4];

  
    pc_MCV = pddc->Dynamic->pPCAttr->modelClipVolume;
    if (!(OC_MCV->operator == PEXModelClipIntersection)) pc_MCV->numObj = 0;     
    /* invalidate flag */
    pddc->Static.misc.flags |= MCVOLUMEFLAG;


		    /* overwrite list */
    OC_HS = (ddHalfSpace *)(OC_MCV->halfspaces->pList);
    count = OC_MCV->halfspaces->numObj;
    for(i = 0; i < count; i++) {	    

	/* transform ref point and vector to world coords */
	/* transform ref point */ 

	miTransformPoint(&OC_HS->orig_point, pddc->Dynamic->mc_to_wc_xform,
                         &tmp_HS.point);

	/* transform ref vector 
	 * Vectors are transformed using the inverse transpose 
	 */

	miMatCopy(pddc->Dynamic->mc_to_wc_xform,
		  vect_xform);

	miMatInverse(vect_xform);

	miMatTranspose(vect_xform);

        miTransformVector(&OC_HS->orig_vector, vect_xform, 
                         &tmp_HS.vector);

	puAddToList(&tmp_HS, 1, pc_MCV);
	OC_HS++;
    } 

    return(Success);
}
 
/*++
 |
 |  Function Name:     miRestoreMCV 
 |
 |  Function Description:
 |      Restores the model clipping volume to that of the
 |    parent, or nil if this is the first structure.  
 |
 |  Note(s):
 |
 --*/
ddpex2rtn
miRestoreMCV(pRend, pOC)
/* in */
    ddRendererPtr       pRend;    /* renderer handle */
    miGenericStr       *pOC;      /* output command */
{


    miDDContext *thispddc = (miDDContext *)(pRend->pDDContext);
    miDynamicDDContext *parentpddc = 
			(miDynamicDDContext *)(thispddc->Dynamic->next);

    if (!(thispddc->Dynamic->next)) /* First structure */
    thispddc->Dynamic->pPCAttr->modelClipVolume->numObj = 0;
    else {
        /* invalidate flag */
        thispddc->Static.misc.flags |= MCVOLUMEFLAG;
	if (puCopyList(parentpddc->pPCAttr->modelClipVolume,
		thispddc->Dynamic->pPCAttr->modelClipVolume ))	
			
	return (BadAlloc);
    }

    return(Success);
}

/*++
 |
 |  Function Name:    miMarkerColourOC
 |
 |  Function Description:
 |     Handles PEXOCMarkerColourIndex and PEXOCMarkerColour.
 |
 |  Note(s):
 |
 --*/
ddpex2rtn
miMarkerColourOC(pRend, pOC)
/* in */
    ddRendererPtr       pRend;      /* renderer handle */
    miGenericStr       *pOC;      /* output command */
/* out */
{
    ddpex3rtn		miConvertColor();

    miDDContext		*pddc = (miDDContext *)(pRend->pDDContext);
    miColourStruct	*pMC = (miColourStruct *)(pOC+1);
    

    switch (pddc->Dynamic->pPCAttr->markerColour.colourType = pMC->colourType)
      {
	case PEXIndexedColour:
	    pddc->Dynamic->pPCAttr->markerColour.colour.indexed = 
							*pMC->colour.pIndex;
	    break;

	case PEXRgbFloatColour:
	case PEXCieFloatColour:
	case PEXHsvFloatColour:
	case PEXHlsFloatColour:
	    pddc->Dynamic->pPCAttr->markerColour.colour.rgbFloat = 
						    *pMC->colour.pRgbFloat;
	    break;

	case PEXRgb8Colour:
	    pddc->Dynamic->pPCAttr->markerColour.colour.rgb8 = 
							*pMC->colour.pRgb8;
	    break;

	case PEXRgb16Colour:
	    pddc->Dynamic->pPCAttr->markerColour.colour.rgb16 = 
							*pMC->colour.pRgb16;
	    break;
    }

    if (!(MI_DDC_IS_HIGHLIGHT(pddc))) {
	if ((pddc->Dynamic->pPCAttr->asfs & PEXMarkerColourAsf) != PEXBundled) {
	    miConvertColor(pRend, 
			   &pddc->Dynamic->pPCAttr->markerColour,
			   pddc->Dynamic->pPCAttr->rdrColourModel,
			   &pddc->Static.attrs->markerColour);
	      pddc->Static.misc.flags |= MARKERGCFLAG;
        }
    }
    return(Success);
}


/*++
 |
 |  Function Name:    miTextColourOC
 |
 |  Function Description:
 |     Handles PEXOCTextColourIndex and PEXOCTextColour.
 |
 |  Note(s):
 |
 --*/
ddpex2rtn
miTextColourOC(pRend, pOC)
/* in */
    ddRendererPtr	pRend;      /* renderer handle */
    miGenericStr	*pOC;      /* output command */
/* out */
{
    ddpex3rtn		miConvertColor();
    miDDContext		*pddc = (miDDContext *)(pRend->pDDContext);
    miColourStruct	*pTC = (miColourStruct *)(pOC+1);

    switch (pddc->Dynamic->pPCAttr->textColour.colourType = pTC->colourType)
      {
	case PEXIndexedColour:
	    pddc->Dynamic->pPCAttr->textColour.colour.indexed = 
							*pTC->colour.pIndex;
	break;

	case PEXRgbFloatColour:
	case PEXCieFloatColour:
	case PEXHsvFloatColour:
	case PEXHlsFloatColour:
	    pddc->Dynamic->pPCAttr->textColour.colour.rgbFloat = 
							*pTC->colour.pRgbFloat;
	    break;

	case PEXRgb8Colour:
	    pddc->Dynamic->pPCAttr->textColour.colour.rgb8 = 
							    *pTC->colour.pRgb8;
	    break;
	case PEXRgb16Colour:
	    pddc->Dynamic->pPCAttr->textColour.colour.rgb16 = 
							    *pTC->colour.pRgb16;
	    break;
    }

    if (!(MI_DDC_IS_HIGHLIGHT(pddc))) {
	if ((pddc->Dynamic->pPCAttr->asfs & PEXTextColourAsf) != PEXBundled) {
	    miConvertColor(pRend, 
			   &pddc->Dynamic->pPCAttr->textColour,
			   pddc->Dynamic->pPCAttr->rdrColourModel,
			   &pddc->Static.attrs->textColour);
	      pddc->Static.misc.flags |= TEXTGCFLAG;
        }
    }
    return(Success);
}


/*++
 |
 |  Function Name:    miLineColourOC
 |
 |  Function Description:
 |     Handles PEXOCLineColourIndex and PEXOCLineColour.
 |
 |  Note(s):
 |
 --*/
ddpex2rtn
miLineColourOC(pRend, pOC)
/* in */
    ddRendererPtr	pRend;      /* renderer handle */
    miGenericStr	*pOC;      /* output command */
/* out */
{
    ddpex3rtn		miConvertColor();
    miDDContext		*pddc = (miDDContext *)(pRend->pDDContext);
    miColourStruct	*pLC = (miColourStruct *)(pOC+1);

    switch (pddc->Dynamic->pPCAttr->lineColour.colourType = pLC->colourType)
      {
	case PEXIndexedColour:
	    pddc->Dynamic->pPCAttr->lineColour.colour.indexed = 
							    *pLC->colour.pIndex;
	    break;

	case PEXRgbFloatColour:
	case PEXCieFloatColour:
	case PEXHsvFloatColour:
	case PEXHlsFloatColour:
	    pddc->Dynamic->pPCAttr->lineColour.colour.rgbFloat = 
							*pLC->colour.pRgbFloat;
	    break;

	case PEXRgb8Colour:
	    pddc->Dynamic->pPCAttr->lineColour.colour.rgb8 = *pLC->colour.pRgb8;
	    break;

	case PEXRgb16Colour:
	    pddc->Dynamic->pPCAttr->lineColour.colour.rgb16 = 
							    *pLC->colour.pRgb16;
	    break;
    }

    if (!(MI_DDC_IS_HIGHLIGHT(pddc))) {
	if ((pddc->Dynamic->pPCAttr->asfs & PEXLineColourAsf) != PEXBundled) {
	    miConvertColor(pRend, 
			   &pddc->Dynamic->pPCAttr->lineColour,
			   pddc->Dynamic->pPCAttr->rdrColourModel,
			   &pddc->Static.attrs->lineColour);
	      pddc->Static.misc.flags |= POLYLINEGCFLAG;
        }
    }
    return(Success);
}

/*++
 |
 |  Function Name:    miSurfaceColourOC
 |
 |  Function Description:
 |     Handles PEXOCSurfaceColourIndex and PEXOCSurfaceColour.
 |
 |  Note(s):
 |
 --*/
ddpex2rtn
miSurfaceColourOC(pRend, pOC)
/* in */
    ddRendererPtr	pRend;      /* renderer handle */
    miGenericStr	*pOC;      /* output command */
/* out */
{
    ddpex3rtn		miConvertColor();
    miDDContext		*pddc = (miDDContext *)(pRend->pDDContext);
    miColourStruct	*pSC = (miColourStruct *)(pOC+1);

    switch (pddc->Dynamic->pPCAttr->surfaceColour.colourType=pSC->colourType)
      {
	case PEXIndexedColour:
	    pddc->Dynamic->pPCAttr->surfaceColour.colour.indexed = 
							*pSC->colour.pIndex;
	    break;

	case PEXRgbFloatColour:
	case PEXCieFloatColour:
	case PEXHsvFloatColour:
	case PEXHlsFloatColour:
	    pddc->Dynamic->pPCAttr->surfaceColour.colour.rgbFloat = 
							*pSC->colour.pRgbFloat;
	    break;
	case PEXRgb8Colour:
	    pddc->Dynamic->pPCAttr->surfaceColour.colour.rgb8 = 
							*pSC->colour.pRgb8;
	    break;

	case PEXRgb16Colour:
	    pddc->Dynamic->pPCAttr->surfaceColour.colour.rgb16 = 
							*pSC->colour.pRgb16;
	    break;
      }

    if (!(MI_DDC_IS_HIGHLIGHT(pddc))) {
	if ((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceColourAsf)!=PEXBundled) {
	    miConvertColor(pRend, 
			   &pddc->Dynamic->pPCAttr->surfaceColour,
			   pddc->Dynamic->pPCAttr->rdrColourModel,
			   &pddc->Static.attrs->surfaceColour);
	      pddc->Static.misc.flags |= FILLAREAGCFLAG;
        }
    }
    return(Success);
}

/*++
 |
 |  Function Name:    miEdgeColourOC
 |
 |  Function Description:
 |     Handles PEXOCSurfaceEdgeColourIndex and PEXOCSurfaceEdgeColour.
 |
 |  Note(s):
 |
 --*/
ddpex2rtn
miEdgeColourOC(pRend, pOC)
/* in */
    ddRendererPtr	pRend;      /* renderer handle */
    miGenericStr	*pOC;      /* output command */
/* out */
{
    ddpex3rtn		miConvertColor();
    miDDContext		*pddc = (miDDContext *)(pRend->pDDContext);
    miColourStruct	*pSEC = (miColourStruct *)(pOC+1);

    switch (pddc->Dynamic->pPCAttr->edgeColour.colourType = pSEC->colourType)
      {
	case PEXIndexedColour:
	    pddc->Dynamic->pPCAttr->edgeColour.colour.indexed = 
							*pSEC->colour.pIndex;
	    break;

	case PEXRgbFloatColour:
	case PEXCieFloatColour:
	case PEXHsvFloatColour:
	case PEXHlsFloatColour:
	    pddc->Dynamic->pPCAttr->edgeColour.colour.rgbFloat = 
							*pSEC->colour.pRgbFloat;
	    break;

	case PEXRgb8Colour:
	    pddc->Dynamic->pPCAttr->edgeColour.colour.rgb8 = *pSEC->colour.pRgb8;
	    break;
	case PEXRgb16Colour:
	    pddc->Dynamic->pPCAttr->edgeColour.colour.rgb16 = 
							*pSEC->colour.pRgb16;
	    break;
      }

    if (!(MI_DDC_IS_HIGHLIGHT(pddc))) {
	if ((pddc->Dynamic->pPCAttr->asfs &PEXSurfaceEdgeColourAsf)!=PEXBundled)
	  {
	    miConvertColor(pRend, 
			   &pddc->Dynamic->pPCAttr->edgeColour,
			   pddc->Dynamic->pPCAttr->rdrColourModel,
			   &pddc->Static.attrs->edgeColour);
	      pddc->Static.misc.flags |= EDGEGCFLAG;
          }
    }
    return(Success);
}
