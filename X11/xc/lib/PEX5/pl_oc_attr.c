/* $Xorg: pl_oc_attr.c,v 1.5 2001/02/09 02:03:28 xorgcvs Exp $ */

/******************************************************************************

Copyright 1992, 1998  The Open Group

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


Copyright 1987,1991 by Digital Equipment Corporation, Maynard, Massachusetts

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and that
the name of Digital not be used in advertising or publicity
pertaining to distribution of the software without specific, written prior
permission.  Digital make no representations about the suitability
of this software for any purpose.  It is provided "as is" without express or
implied warranty.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************************/

#include "PEXlib.h"
#include "PEXlibint.h"
#include "pl_oc_util.h"


void
PEXSetMarkerType (display, resource_id, req_type, type)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		type;

{
    register pexMarkerType	*req;

    BEGIN_SIMPLE_OC (MarkerType, resource_id, req_type, req);
    req->markerType = type;
    END_SIMPLE_OC (MarkerType, resource_id, req_type, req);
}


void
PEXSetMarkerScale (display, resource_id, req_type, scale)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT double		scale;

{
    register pexMarkerScale	*req;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (MarkerScale, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_DHTON (scale, req->scale, fpFormat);
    }
    else
	req->scale = scale;

    END_SIMPLE_OC (MarkerScale, resource_id, req_type, req);
}


void
PEXSetMarkerColorIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexMarkerColorIndex	*req;

    BEGIN_SIMPLE_OC (MarkerColorIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (MarkerColorIndex, resource_id, req_type, req);
}


void
PEXSetMarkerColor (display, resource_id, req_type, colorType, color)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		colorType;
INPUT PEXColor		*color;

{
    register pexMarkerColor	*req;
    char			*pBuf;
    int				lenofColor;
    int				fpConvert;
    int				fpFormat;

    lenofColor = GetColorLength (colorType);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexMarkerColor), lenofColor, pBuf);

    if (pBuf == NULL) return;

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (MarkerColor, lenofColor, pBuf, req);

    req->colorType = colorType;

    END_OC_HEADER (MarkerColor, pBuf, req);

    pBuf = PEXGetOCAddr (display, NUMBYTES (lenofColor));
    STORE_COLOR_VAL (colorType, (*color), pBuf, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetMarkerBundleIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexMarkerBundleIndex	*req;

    BEGIN_SIMPLE_OC (MarkerBundleIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (MarkerBundleIndex, resource_id, req_type, req);
}


void
PEXSetTextFontIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexTextFontIndex	*req;

    BEGIN_SIMPLE_OC (TextFontIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (TextFontIndex, resource_id, req_type, req);
}


void
PEXSetTextPrecision (display, resource_id, req_type, precision)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		precision;

{
    register pexTextPrecision	*req;

    BEGIN_SIMPLE_OC (TextPrecision, resource_id, req_type, req);
    req->precision = precision;
    END_SIMPLE_OC (TextPrecision, resource_id, req_type, req);
}


void
PEXSetCharExpansion (display, resource_id, req_type, expansion)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT double		expansion;

{
    register pexCharExpansion	*req;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (CharExpansion, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_DHTON (expansion, req->expansion, fpFormat);
    }
    else
	req->expansion = expansion;

    END_SIMPLE_OC (CharExpansion, resource_id, req_type, req);
}


void
PEXSetCharSpacing (display, resource_id, req_type, spacing)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT double		spacing;

{
    register pexCharSpacing	*req;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (CharSpacing, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_DHTON (spacing, req->spacing, fpFormat);
    }
    else
	req->spacing = spacing;

    END_SIMPLE_OC (CharSpacing, resource_id, req_type, req);
}


void
PEXSetTextColorIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexTextColorIndex	*req;

    BEGIN_SIMPLE_OC (TextColorIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (TextColorIndex, resource_id, req_type, req);
}


void
PEXSetTextColor (display, resource_id, req_type, colorType, color)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		colorType;
INPUT PEXColor		*color;

{
    register pexTextColor	*req;
    char			*pBuf;
    int				lenofColor;
    int				fpConvert;
    int				fpFormat;

    lenofColor = GetColorLength (colorType);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexTextColor), lenofColor, pBuf);

    if (pBuf == NULL) return;

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (TextColor, lenofColor, pBuf, req);

    req->colorType = colorType;

    END_OC_HEADER (TextColor, pBuf, req);

    pBuf = PEXGetOCAddr (display, NUMBYTES (lenofColor));
    STORE_COLOR_VAL (colorType, (*color), pBuf, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetCharHeight (display, resource_id, req_type, height)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT double		height;

{
    register pexCharHeight	*req;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (CharHeight, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_DHTON (height, req->height, fpFormat);
    }
    else
	req->height = height;

    END_SIMPLE_OC (CharHeight, resource_id, req_type, req);
}


void
PEXSetCharUpVector (display, resource_id, req_type, vector)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXVector2D	*vector;

{
    register pexCharUpVector	*req;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (CharUpVector, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_HTON (vector->x, req->up_x, fpFormat);
	FP_CONVERT_HTON (vector->y, req->up_y, fpFormat);
    }
    else
    {
	req->up_x = vector->x;
	req->up_y = vector->y;
    }

    END_SIMPLE_OC (CharUpVector, resource_id, req_type, req);
}


void
PEXSetTextPath (display, resource_id, req_type, path)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		path;

{
    register pexTextPath	*req;

    BEGIN_SIMPLE_OC (TextPath, resource_id, req_type, req);
    req->path = path;
    END_SIMPLE_OC (TextPath, resource_id, req_type, req);
}


void
PEXSetTextAlignment (display, resource_id, req_type, halignment, valignment)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		halignment;
INPUT int		valignment;

{
    register pexTextAlignment	*req;

    BEGIN_SIMPLE_OC (TextAlignment, resource_id, req_type, req);
    req->alignment_vertical = valignment;
    req->alignment_horizontal = halignment;
    END_SIMPLE_OC (TextAlignment, resource_id, req_type, req);
}


void
PEXSetATextHeight (display, resource_id, req_type, height)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT double		height;

{
    register pexATextHeight	*req;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (ATextHeight, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_DHTON (height, req->height, fpFormat);
    }
    else
	req->height = height;

    END_SIMPLE_OC (ATextHeight, resource_id, req_type, req);
}


void
PEXSetATextUpVector (display, resource_id, req_type, vector)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXVector2D	*vector;

{
    register pexATextUpVector	*req;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (ATextUpVector, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_HTON (vector->x, req->up_x, fpFormat);
	FP_CONVERT_HTON (vector->y, req->up_y, fpFormat);
    }
    else
    {
	req->up_x = vector->x;
	req->up_y = vector->y;
    }

    END_SIMPLE_OC (ATextUpVector, resource_id, req_type, req);
}


void
PEXSetATextPath (display, resource_id, req_type, path)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		path;

{
    register pexATextPath	*req;

    BEGIN_SIMPLE_OC (ATextPath, resource_id, req_type, req);
    req->path = path;
    END_SIMPLE_OC (ATextPath, resource_id, req_type, req);
}


void
PEXSetATextAlignment (display, resource_id, req_type, halignment, valignment)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		halignment;
INPUT int		valignment;

{
    register pexATextAlignment	*req;

    BEGIN_SIMPLE_OC (ATextAlignment, resource_id, req_type, req);
    req->alignment_vertical = valignment;
    req->alignment_horizontal = halignment;
    END_SIMPLE_OC (ATextAlignment, resource_id, req_type, req);
}


void
PEXSetATextStyle (display, resource_id, req_type, style)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		style;

{
    register pexATextStyle	*req;

    BEGIN_SIMPLE_OC (ATextStyle, resource_id, req_type, req);
    req->style = style;
    END_SIMPLE_OC (ATextStyle, resource_id, req_type, req);
}


void
PEXSetTextBundleIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexTextBundleIndex	*req;

    BEGIN_SIMPLE_OC (TextBundleIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (TextBundleIndex, resource_id, req_type, req);
}


void
PEXSetLineType (display, resource_id, req_type, type)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		type;

{
    register pexLineType	*req;

    BEGIN_SIMPLE_OC (LineType, resource_id, req_type, req);
    req->lineType = type;
    END_SIMPLE_OC (LineType, resource_id, req_type, req);
}


void
PEXSetLineWidth (display, resource_id, req_type, width)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT double		width;

{
    register pexLineWidth	*req;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (LineWidth, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_DHTON (width, req->width, fpFormat);
    }
    else
	req->width = width;

    END_SIMPLE_OC (LineWidth, resource_id, req_type, req);
}


void
PEXSetLineColorIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexLineColorIndex	*req;

    BEGIN_SIMPLE_OC (LineColorIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (LineColorIndex, resource_id, req_type, req);
}


void
PEXSetLineColor (display, resource_id, req_type, colorType, color)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		colorType;
INPUT PEXColor		*color;

{
    register pexLineColor	*req;
    char			*pBuf;
    int				lenofColor;
    int				fpConvert;
    int				fpFormat;

    lenofColor = GetColorLength (colorType);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexLineColor), lenofColor, pBuf);

    if (pBuf == NULL) return;

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (LineColor, lenofColor, pBuf, req);

    req->colorType = colorType;

    END_OC_HEADER (LineColor, pBuf, req);

    pBuf = PEXGetOCAddr (display, NUMBYTES (lenofColor));
    STORE_COLOR_VAL (colorType, (*color), pBuf, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetCurveApprox (display, resource_id, req_type, approxMethod, tolerance)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		approxMethod;
INPUT double		tolerance;

{
    register pexCurveApprox	*req;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (CurveApprox, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    req->approxMethod = approxMethod;

    if (fpConvert)
    {
	FP_CONVERT_DHTON (tolerance, req->tolerance, fpFormat);
    }
    else
	req->tolerance = tolerance;

    END_SIMPLE_OC (CurveApprox, resource_id, req_type, req);
}


void
PEXSetPolylineInterpMethod (display, resource_id, req_type, method)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		method;

{
    register pexPolylineInterpMethod	*req;

    BEGIN_SIMPLE_OC (PolylineInterpMethod, resource_id, req_type, req);
    req->polylineInterp = method;
    END_SIMPLE_OC (PolylineInterpMethod, resource_id, req_type, req);
}


void
PEXSetLineBundleIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexLineBundleIndex	*req;

    BEGIN_SIMPLE_OC (LineBundleIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (LineBundleIndex, resource_id, req_type, req);
}


void
PEXSetInteriorStyle (display, resource_id, req_type, style)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		style;

{
    register pexInteriorStyle	*req;

    BEGIN_SIMPLE_OC (InteriorStyle, resource_id, req_type, req);
    req->interiorStyle = style;
    END_SIMPLE_OC (InteriorStyle, resource_id, req_type, req);
}


void
PEXSetInteriorStyleIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		index;

{
    register pexInteriorStyleIndex	*req;

    BEGIN_SIMPLE_OC (InteriorStyleIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (InteriorStyleIndex, resource_id, req_type, req);
}


void
PEXSetSurfaceColorIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexSurfaceColorIndex	*req;

    BEGIN_SIMPLE_OC (SurfaceColorIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (SurfaceColorIndex, resource_id, req_type, req);
}


void
PEXSetSurfaceColor (display, resource_id, req_type, colorType, color)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		colorType;
INPUT PEXColor		*color;

{
    register pexSurfaceColor	*req;
    char			*pBuf;
    int				lenofColor;
    int				fpConvert;
    int				fpFormat;

    lenofColor = GetColorLength (colorType);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexSurfaceColor), lenofColor, pBuf);

    if (pBuf == NULL) return;

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (SurfaceColor, lenofColor, pBuf, req);

    req->colorType = colorType;

    END_OC_HEADER (SurfaceColor, pBuf, req);

    pBuf = PEXGetOCAddr (display, NUMBYTES (lenofColor));
    STORE_COLOR_VAL (colorType, (*color), pBuf, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetReflectionAttributes (display, resource_id, req_type, reflectionAttr)

INPUT Display			*display;
INPUT XID			resource_id;
INPUT PEXOCRequestType		req_type;
INPUT PEXReflectionAttributes	*reflectionAttr;

{
    register pexReflectionAttributes	*req;
    char				*pBuf;
    int					lenofColor;
    int					fpConvert;
    int					fpFormat;

    lenofColor = GetColorLength (reflectionAttr->specular_color.type);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexReflectionAttributes), lenofColor, pBuf);

    if (pBuf == NULL) return;

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (ReflectionAttributes, lenofColor, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (reflectionAttr->ambient, req->ambient, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->diffuse, req->diffuse, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->specular, req->specular, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->specular_conc,
	    req->specularConc, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->transmission,
	    req->transmission, fpFormat);
    }
    else
    {
	req->ambient = reflectionAttr->ambient;
	req->diffuse = reflectionAttr->diffuse;
	req->specular = reflectionAttr->specular;
	req->specularConc = reflectionAttr->specular_conc;
	req->transmission = reflectionAttr->transmission;
    }

    req->specular_colorType = reflectionAttr->specular_color.type;

    END_OC_HEADER (ReflectionAttributes, pBuf, req);

    pBuf = PEXGetOCAddr (display, NUMBYTES (lenofColor));
    STORE_COLOR_VAL (reflectionAttr->specular_color.type,
	reflectionAttr->specular_color.value, pBuf, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetReflectionModel (display, resource_id, req_type, model)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		model;

{
    register pexReflectionModel	*req;

    BEGIN_SIMPLE_OC (ReflectionModel, resource_id, req_type, req);
    req->reflectionModel = model;
    END_SIMPLE_OC (ReflectionModel, resource_id, req_type, req);
}


void
PEXSetSurfaceInterpMethod (display, resource_id, req_type, method)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		method;

{
    register pexSurfaceInterpMethod	*req;

    BEGIN_SIMPLE_OC (SurfaceInterpMethod, resource_id, req_type, req);
    req->surfaceInterp = method;
    END_SIMPLE_OC (SurfaceInterpMethod, resource_id, req_type, req);
}


void
PEXSetBFInteriorStyle (display, resource_id, req_type, style)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		style;

{
    register pexBFInteriorStyle	*req;

    BEGIN_SIMPLE_OC (BFInteriorStyle, resource_id, req_type, req);
    req->interiorStyle = style;
    END_SIMPLE_OC (BFInteriorStyle, resource_id, req_type, req);
}


void
PEXSetBFInteriorStyleIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		index;

{
    register pexBFInteriorStyleIndex	*req;

    BEGIN_SIMPLE_OC (BFInteriorStyleIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (BFInteriorStyleIndex, resource_id, req_type, req);
}


void
PEXSetBFSurfaceColorIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexBFSurfaceColorIndex	*req;

    BEGIN_SIMPLE_OC (BFSurfaceColorIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (BFSurfaceColorIndex, resource_id, req_type, req);
}


void
PEXSetBFSurfaceColor (display, resource_id, req_type, colorType, color)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		colorType;
INPUT PEXColor		*color;

{
    register pexBFSurfaceColor	*req;
    char			*pBuf;
    int				lenofColor;
    int				fpConvert;
    int				fpFormat;

    lenofColor = GetColorLength (colorType);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexBFSurfaceColor), lenofColor, pBuf);

    if (pBuf == NULL) return;

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (BFSurfaceColor, lenofColor, pBuf, req);

    req->colorType = colorType;

    END_OC_HEADER (BFSurfaceColor, pBuf, req);

    pBuf = PEXGetOCAddr (display, NUMBYTES (lenofColor));
    STORE_COLOR_VAL (colorType, (*color), pBuf, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetBFReflectionAttributes (display, resource_id, req_type, reflectionAttr)

INPUT Display			*display;
INPUT XID			resource_id;
INPUT PEXOCRequestType		req_type;
INPUT PEXReflectionAttributes	*reflectionAttr;

{
    register pexBFReflectionAttributes	*req;
    char				*pBuf;
    int					lenofColor;
    int					fpConvert;
    int					fpFormat;

    lenofColor = GetColorLength (reflectionAttr->specular_color.type);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexBFReflectionAttributes), lenofColor, pBuf);

    if (pBuf == NULL) return;

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (BFReflectionAttributes, lenofColor, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (reflectionAttr->ambient, req->ambient, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->diffuse, req->diffuse, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->specular, req->specular, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->specular_conc,
	    req->specularConc, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->transmission,
	    req->transmission, fpFormat);
    }
    else
    {
	req->ambient = reflectionAttr->ambient;
	req->diffuse = reflectionAttr->diffuse;
	req->specular = reflectionAttr->specular;
	req->specularConc = reflectionAttr->specular_conc;
	req->transmission = reflectionAttr->transmission;
    }

    req->specular_colorType = reflectionAttr->specular_color.type;

    END_OC_HEADER (BFReflectionAttributes, pBuf, req);

    pBuf = PEXGetOCAddr (display, NUMBYTES (lenofColor));
    STORE_COLOR_VAL (reflectionAttr->specular_color.type,
	reflectionAttr->specular_color.value, pBuf, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetBFReflectionModel (display, resource_id, req_type, model)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		model;

{
    register pexBFReflectionModel	*req;

    BEGIN_SIMPLE_OC (BFReflectionModel, resource_id, req_type, req);
    req->reflectionModel = model;
    END_SIMPLE_OC (BFReflectionModel, resource_id, req_type, req);
}


void
PEXSetBFSurfaceInterpMethod (display, resource_id, req_type, method)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		method;

{
    register pexBFSurfaceInterpMethod	*req;

    BEGIN_SIMPLE_OC (BFSurfaceInterpMethod, resource_id, req_type, req);
    req->surfaceInterp = method;
    END_SIMPLE_OC (BFSurfaceInterpMethod, resource_id, req_type, req);
}


void
PEXSetSurfaceApprox (display, resource_id, req_type,
    approxMethod, uTolerance, vTolerance)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		approxMethod;
INPUT double		uTolerance;
INPUT double		vTolerance;

{
    register pexSurfaceApprox	*req;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (SurfaceApprox, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    req->approxMethod = approxMethod;

    if (fpConvert)
    {
	FP_CONVERT_DHTON (uTolerance, req->uTolerance, fpFormat);
	FP_CONVERT_DHTON (vTolerance, req->vTolerance, fpFormat);
    }
    else
    {
	req->uTolerance = uTolerance;
	req->vTolerance = vTolerance;
    }

    END_SIMPLE_OC (SurfaceApprox, resource_id, req_type, req);
}


void
PEXSetFacetCullingMode (display, resource_id, req_type, mode)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		mode;

{
    register pexFacetCullingMode	*req;

    BEGIN_SIMPLE_OC (FacetCullingMode, resource_id, req_type, req);
    req->cullMode = mode;
    END_SIMPLE_OC (FacetCullingMode, resource_id, req_type, req);
}


void
PEXSetFacetDistinguishFlag (display, resource_id, req_type, flag)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		flag;

{
    register pexFacetDistinguishFlag	*req;

    BEGIN_SIMPLE_OC (FacetDistinguishFlag, resource_id, req_type, req);
    req->distinguish = flag;
    END_SIMPLE_OC (FacetDistinguishFlag, resource_id, req_type, req);
}


void
PEXSetPatternSize (display, resource_id, req_type, width, height)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT double		width;
INPUT double		height;

{
    register pexPatternSize	*req;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (PatternSize, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_DHTON (width, req->size_x, fpFormat);
	FP_CONVERT_DHTON (height, req->size_y, fpFormat);
    }
    else
    {
	req->size_x = width;
	req->size_y = height;
    }

    END_SIMPLE_OC (PatternSize, resource_id, req_type, req);
}


void
PEXSetPatternAttributes2D (display, resource_id, req_type, ref_point)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXCoord2D	*ref_point;

{
    register pexPatternAttributes2D	*req;
    int					fpConvert;
    int					fpFormat;

    BEGIN_SIMPLE_OC (PatternAttributes2D, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ref_point->x, req->point_x, fpFormat);
	FP_CONVERT_HTON (ref_point->y, req->point_y, fpFormat);
    }
    else
    {
	req->point_x = ref_point->x;
	req->point_y = ref_point->y;
    }

    END_SIMPLE_OC (PatternAttributes2D, resource_id, req_type, req);
}


void
PEXSetPatternAttributes (display, resource_id, req_type, refPt, vec1, vec2)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXCoord  	*refPt;
INPUT PEXVector 	*vec1;
INPUT PEXVector 	*vec2;

{
    register pexPatternAttributes	*req;
    int					fpConvert;
    int					fpFormat;

    BEGIN_SIMPLE_OC (PatternAttributes, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_HTON (refPt->x, req->refPt_x, fpFormat);
	FP_CONVERT_HTON (refPt->y, req->refPt_y, fpFormat);
	FP_CONVERT_HTON (refPt->z, req->refPt_z, fpFormat);
	FP_CONVERT_HTON (vec1->x, req->vector1_x, fpFormat);
	FP_CONVERT_HTON (vec1->y, req->vector1_y, fpFormat);
	FP_CONVERT_HTON (vec1->z, req->vector1_z, fpFormat);
	FP_CONVERT_HTON (vec2->x, req->vector2_x, fpFormat);
	FP_CONVERT_HTON (vec2->y, req->vector2_y, fpFormat);
	FP_CONVERT_HTON (vec2->z, req->vector2_z, fpFormat);
    }
    else
    {
	req->refPt_x = refPt->x;
	req->refPt_y = refPt->y;
	req->refPt_z = refPt->z;
	req->vector1_x = vec1->x;
	req->vector1_y = vec1->y;
	req->vector1_z = vec1->z;
	req->vector2_x = vec2->x;
	req->vector2_y = vec2->y;
	req->vector2_z = vec2->z;
    }

    END_SIMPLE_OC (PatternAttributes, resource_id, req_type, req);
}


void
PEXSetInteriorBundleIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexInteriorBundleIndex	*req;

    BEGIN_SIMPLE_OC (InteriorBundleIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (InteriorBundleIndex, resource_id, req_type, req);
}


void
PEXSetSurfaceEdgeFlag (display, resource_id, req_type, flag)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		flag;

{
    register pexSurfaceEdgeFlag	*req;

    BEGIN_SIMPLE_OC (SurfaceEdgeFlag, resource_id, req_type, req);
    req->onoff = flag;
    END_SIMPLE_OC (SurfaceEdgeFlag, resource_id, req_type, req);
}


void
PEXSetSurfaceEdgeType (display, resource_id, req_type, type)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		type;

{
    register pexSurfaceEdgeType	*req;

    BEGIN_SIMPLE_OC (SurfaceEdgeType, resource_id, req_type, req);
    req->edgeType = type;
    END_SIMPLE_OC (SurfaceEdgeType, resource_id, req_type, req);
}


void
PEXSetSurfaceEdgeWidth (display, resource_id, req_type, width)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT double		width;

{
    register pexSurfaceEdgeWidth	*req;
    int					fpConvert;
    int					fpFormat;

    BEGIN_SIMPLE_OC (SurfaceEdgeWidth, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    if (fpConvert)
    {
	FP_CONVERT_DHTON (width, req->width, fpFormat);
    }
    else
	req->width = width;

    END_SIMPLE_OC (SurfaceEdgeWidth, resource_id, req_type, req);
}


void
PEXSetSurfaceEdgeColorIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexSurfaceEdgeColorIndex	*req;

    BEGIN_SIMPLE_OC (SurfaceEdgeColorIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (SurfaceEdgeColorIndex, resource_id, req_type, req);
}


void
PEXSetSurfaceEdgeColor (display, resource_id, req_type, colorType, color)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		colorType;
INPUT PEXColor		*color;

{
    register pexSurfaceEdgeColor	*req;
    char				*pBuf;
    int					lenofColor;
    int					fpConvert;
    int					fpFormat;

    lenofColor = GetColorLength (colorType);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexSurfaceEdgeColor), lenofColor, pBuf);

    if (pBuf == NULL) return;

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (SurfaceEdgeColor, lenofColor, pBuf, req);

    req->colorType = colorType;

    END_OC_HEADER (SurfaceEdgeColor, pBuf, req);

    pBuf = PEXGetOCAddr (display, NUMBYTES (lenofColor));
    STORE_COLOR_VAL (colorType, (*color), pBuf, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetEdgeBundleIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexEdgeBundleIndex	*req;

    BEGIN_SIMPLE_OC (EdgeBundleIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (EdgeBundleIndex, resource_id, req_type, req);
}


void
PEXSetIndividualASF (display, resource_id, req_type, attribute, value)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned long	attribute;
INPUT int		value;

{
    register pexIndividualASF	*req;

    BEGIN_SIMPLE_OC (IndividualASF, resource_id, req_type, req);
    req->attribute = attribute;
    req->source = value;
    END_SIMPLE_OC (IndividualASF, resource_id, req_type, req);
}


void
PEXSetLocalTransform (display, resource_id, req_type, compType, transform)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		compType;
INPUT PEXMatrix		transform;

{
    register pexLocalTransform	*req;
    char			*ptr;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (LocalTransform, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    req->compType = compType;

    ptr = (char *) req->matrix;
    STORE_LISTOF_FLOAT32 (16, transform, ptr, fpConvert, fpFormat);

    END_SIMPLE_OC (LocalTransform, resource_id, req_type, req);
}


void
PEXSetLocalTransform2D (display, resource_id, req_type, compType, transform)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		compType;
INPUT PEXMatrix3x3	transform;

{
    register pexLocalTransform2D	*req;
    char				*ptr;
    int					fpConvert;
    int					fpFormat;

    BEGIN_SIMPLE_OC (LocalTransform2D, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    req->compType = compType;

    ptr = (char *) req->matrix3X3;
    STORE_LISTOF_FLOAT32 (9, transform, ptr, fpConvert, fpFormat);

    END_SIMPLE_OC (LocalTransform2D, resource_id, req_type, req);
}


void
PEXSetGlobalTransform (display, resource_id, req_type, transform)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXMatrix		transform;

{
    register pexGlobalTransform	*req;
    char			*ptr;
    int				fpConvert;
    int				fpFormat;

    BEGIN_SIMPLE_OC (GlobalTransform, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    ptr = (char *) req->matrix;
    STORE_LISTOF_FLOAT32 (16, transform, ptr, fpConvert, fpFormat);

    END_SIMPLE_OC (GlobalTransform, resource_id, req_type, req);
}


void
PEXSetGlobalTransform2D (display, resource_id, req_type, transform)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXMatrix3x3	transform;

{
    register pexGlobalTransform2D	*req;
    char				*ptr;
    int					fpConvert;
    int					fpFormat;

    BEGIN_SIMPLE_OC (GlobalTransform2D, resource_id, req_type, req);
    CHECK_FP (fpConvert, fpFormat);

    ptr = (char *) req->matrix3X3;
    STORE_LISTOF_FLOAT32 (9, transform, ptr, fpConvert, fpFormat);

    END_SIMPLE_OC (GlobalTransform2D, resource_id, req_type, req);
}


void
PEXSetModelClipFlag (display, resource_id, req_type, flag)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		flag;

{
    register pexModelClipFlag	*req;

    BEGIN_SIMPLE_OC (ModelClipFlag, resource_id, req_type, req);
    req->onoff = flag;
    END_SIMPLE_OC (ModelClipFlag, resource_id, req_type, req);
}


void
PEXSetModelClipVolume (display, resource_id, req_type, op,
    numHalfSpaces, halfSpaces)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		op;
INPUT unsigned int	numHalfSpaces;
INPUT PEXHalfSpace	*halfSpaces;

{
    register pexModelClipVolume *req;
    char			*pBuf;
    int				dataLength;
    int				fpConvert;
    int				fpFormat;

    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (numHalfSpaces * SIZEOF (pexHalfSpace));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexModelClipVolume), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (ModelClipVolume, dataLength, pBuf, req);

    req->modelClipOperator = op; 
    req->numHalfSpaces = numHalfSpaces;

    END_OC_HEADER (ModelClipVolume, pBuf, req);


    /*
     * Copy the oc data.
     */

    OC_LISTOF_HALFSPACE3D (numHalfSpaces, halfSpaces, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetModelClipVolume2D (display, resource_id, req_type, op,
    numHalfSpaces, halfSpaces)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		op;
INPUT unsigned int	numHalfSpaces;
INPUT PEXHalfSpace2D	*halfSpaces;

{
    register pexModelClipVolume2D	*req;
    char				*pBuf;
    int					dataLength;
    int					fpConvert;
    int					fpFormat;

    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (numHalfSpaces * SIZEOF (pexHalfSpace2D));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexModelClipVolume2D), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (ModelClipVolume2D, dataLength, pBuf, req);

    req->modelClipOperator = op; 
    req->numHalfSpaces = numHalfSpaces;

    END_OC_HEADER (ModelClipVolume2D, pBuf, req);


    /*
     * Copy the oc data.
     */

    OC_LISTOF_HALFSPACE2D (numHalfSpaces, halfSpaces, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXRestoreModelClipVolume (display, resource_id, req_type)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;

{
    register pexRestoreModelClipVolume	*req;

    BEGIN_SIMPLE_OC (RestoreModelClipVolume, resource_id, req_type, req);
    /* no data */
    END_SIMPLE_OC (RestoreModelClipVolume, resource_id, req_type, req);
}


void
PEXSetViewIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexViewIndex	*req;

    BEGIN_SIMPLE_OC (ViewIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (ViewIndex, resource_id, req_type, req);
}


void
PEXSetLightSourceState (display, resource_id, req_type,
    numEnable, enable, numDisable, disable)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	numEnable;
INPUT PEXTableIndex	*enable;
INPUT unsigned int	numDisable;
INPUT PEXTableIndex	*disable;

{
    register pexLightSourceState	*req;
    char				*pBuf;
    int					dataLength;

    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (numEnable * SIZEOF (CARD16)) +
	NUMWORDS (numDisable * SIZEOF (CARD16));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexLightSourceState), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the request header data. 
     */

    BEGIN_OC_HEADER (LightSourceState, dataLength, pBuf, req);

    req->numEnable = numEnable;
    req->numDisable = numDisable;

    END_OC_HEADER (LightSourceState, pBuf, req);


    /*
     * Copy the oc data.
     */

    OC_LISTOF_CARD16_PAD (numEnable, enable);
    OC_LISTOF_CARD16_PAD (numDisable, disable);


    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetDepthCueIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexDepthCueIndex	*req;

    BEGIN_SIMPLE_OC (DepthCueIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (DepthCueIndex, resource_id, req_type, req);
}


void
PEXSetPickID (display, resource_id, req_type, id)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned long	id;

{
    register pexPickID	*req;

    BEGIN_SIMPLE_OC (PickID, resource_id, req_type, req);
    req->pickId = id;
    END_SIMPLE_OC (PickID, resource_id, req_type, req);
}


void
PEXSetHLHSRID (display, resource_id, req_type, id)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned long	id;

{
    register pexHLHSRID	*req;

    BEGIN_SIMPLE_OC (HLHSRID, resource_id, req_type, req);
    req->hlhsrID = id;
    END_SIMPLE_OC (HLHSRID, resource_id, req_type, req);
}


void
PEXSetColorApproxIndex (display, resource_id, req_type, index)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	index;

{
    register pexColorApproxIndex	*req;

    BEGIN_SIMPLE_OC (ColorApproxIndex, resource_id, req_type, req);
    req->index = index;
    END_SIMPLE_OC (ColorApproxIndex, resource_id, req_type, req);
}


void
PEXSetParaSurfCharacteristics (display, resource_id, req_type,
    pscType, pscData)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		pscType;
INPUT PEXPSCData	*pscData;

{
    register pexParaSurfCharacteristics *req;
    char				*pBuf;
    int					dataLength = 0;
    int					fpConvert;
    int					fpFormat;

    /*
     * Initialize the OC request.
     */

    if (pscType == PEXPSCIsoCurves)
    {
	dataLength = LENOF (pexPSC_IsoparametricCurves);
    }
    else if (pscType == PEXPSCMCLevelCurves || pscType == PEXPSCWCLevelCurves)
    {
	dataLength = NUMWORDS (SIZEOF (pexPSC_LevelCurves) +
		(pscData->level_curves.count * SIZEOF (float)));
    }

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexParaSurfCharacteristics), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (ParaSurfCharacteristics, dataLength, pBuf, req);

    req->characteristics = pscType;
    req->length = NUMBYTES (dataLength);

    END_OC_HEADER (ParaSurfCharacteristics, pBuf, req);


    /*
     * Copy the oc data.
     */

    if ((pBuf = PEXGetOCAddr (display, req->length)))
    {
	if (pscType == PEXPSCIsoCurves)
	{
	    STORE_PSC_ISOCURVES (pscData->iso_curves, pBuf);
	}
	else if (pscType == PEXPSCMCLevelCurves ||
	    pscType == PEXPSCWCLevelCurves)
	{
	    STORE_PSC_LEVELCURVES (pscData->level_curves, pBuf,
		fpConvert, fpFormat);

	    STORE_LISTOF_FLOAT32 (pscData->level_curves.count,
		pscData->level_curves.parameters, pBuf, fpConvert, fpFormat);
	}
    }

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetRenderingColorModel (display, resource_id, req_type, model)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		model;

{
    register pexRenderingColorModel	*req;

    BEGIN_SIMPLE_OC (RenderingColorModel, resource_id, req_type, req);
    req->model = model;
    END_SIMPLE_OC (RenderingColorModel, resource_id, req_type, req);
}


void
PEXAddToNameSet (display, resource_id, req_type, numNames, names)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned long	numNames;
INPUT PEXName		*names;

{
    register pexAddToNameSet	*req;
    char			*pBuf;
    int				dataLength;

    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (numNames * SIZEOF (pexName));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexAddToNameSet), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the request header data. 
     */

    BEGIN_OC_HEADER (AddToNameSet, dataLength, pBuf, req);
    END_OC_HEADER (AddToNameSet, pBuf, req);


    /*
     * Copy the oc data.
     */

    OC_LISTOF_CARD32 (numNames, names);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXRemoveFromNameSet (display, resource_id, req_type, numNames, names)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned long	numNames;
INPUT PEXName		*names;

{
    register pexRemoveFromNameSet	*req;
    char				*pBuf;
    int					dataLength;

    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (numNames * SIZEOF (pexName));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexRemoveFromNameSet), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the request header data. 
     */

    BEGIN_OC_HEADER (RemoveFromNameSet, dataLength, pBuf, req);
    END_OC_HEADER (RemoveFromNameSet, pBuf, req);


    /*
     * Copy the oc data.
     */

    OC_LISTOF_CARD32 (numNames, names);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}
