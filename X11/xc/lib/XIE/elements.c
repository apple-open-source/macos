/* $Xorg: elements.c,v 1.6 2001/02/09 02:03:41 xorgcvs Exp $ */

/*

Copyright 1993, 1994, 1998  The Open Group

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
/* $XFree86: xc/lib/XIE/elements.c,v 1.5 2001/12/14 19:54:33 dawes Exp $ */

#include "XIElibint.h"
#include "elements.h"
#include "globals.h"

#include <stdio.h>


int
_XiePhotofloSize (
	XiePhotoElement	*elem_list,
	int		elem_count)
{
    XiePhotoElement	*elemSrc;
    int 		size = 0;
    int 		i;

    for (i = 0; i < elem_count; i++)
    {
	elemSrc = &elem_list[i];

	switch (elemSrc->elemType)
	{
	case xieElemImportClientLUT:
	    size += SIZEOF (xieFloImportClientLUT);
	    break;

	case xieElemImportClientPhoto:
	    size += SIZEOF (xieFloImportClientPhoto) +
		(_XieTechniqueLength (xieValDecode,
		elemSrc->data.ImportClientPhoto.decode_tech,
		elemSrc->data.ImportClientPhoto.decode_param) << 2);
	    break;

	case xieElemImportClientROI:
	    size += SIZEOF (xieFloImportClientROI);
	    break;

	case xieElemImportDrawable:
	    size += SIZEOF (xieFloImportDrawable);
	    break;

	case xieElemImportDrawablePlane:
	    size += SIZEOF (xieFloImportDrawablePlane);
	    break;

	case xieElemImportLUT:
	    size += SIZEOF (xieFloImportLUT);
	    break;

	case xieElemImportPhotomap:
	    size += SIZEOF (xieFloImportPhotomap);
	    break;

	case xieElemImportROI:
	    size += SIZEOF (xieFloImportROI);
	    break;

	case xieElemArithmetic:
	    size += SIZEOF (xieFloArithmetic);
	    break;

	case xieElemBandCombine:
	    size += SIZEOF (xieFloBandCombine);
	    break;

	case xieElemBandExtract:
	    size += SIZEOF (xieFloBandExtract);
	    break;

	case xieElemBandSelect:
	    size += SIZEOF (xieFloBandSelect);
	    break;

	case xieElemBlend:
	    size += SIZEOF (xieFloBlend);
	    break;

	case xieElemCompare:
	    size += SIZEOF (xieFloCompare);
	    break;

	case xieElemConstrain:
	    size += SIZEOF (xieFloConstrain) +
		(_XieTechniqueLength (xieValConstrain,
		elemSrc->data.Constrain.constrain_tech,
		elemSrc->data.Constrain.constrain_param) << 2);
	    break;

	case xieElemConvertFromIndex:
	    size += SIZEOF (xieFloConvertFromIndex);
	    break;

	case xieElemConvertFromRGB:
	    size += SIZEOF (xieFloConvertFromRGB) +
		(_XieTechniqueLength (xieValConvertFromRGB,
		elemSrc->data.ConvertFromRGB.color_space,
		elemSrc->data.ConvertFromRGB.color_param) << 2);
	    break;

	case xieElemConvertToIndex:
	    size += SIZEOF (xieFloConvertToIndex) +
		(_XieTechniqueLength (xieValColorAlloc,
		elemSrc->data.ConvertToIndex.color_alloc_tech,
		elemSrc->data.ConvertToIndex.color_alloc_param) << 2);
	    break;

	case xieElemConvertToRGB:
	    size += SIZEOF (xieFloConvertToRGB) +
		(_XieTechniqueLength (xieValConvertToRGB,
		elemSrc->data.ConvertToRGB.color_space,
		elemSrc->data.ConvertToRGB.color_param) << 2);
	    break;

	case xieElemConvolve:
	    size += SIZEOF (xieFloConvolve) +
		(4 * elemSrc->data.Convolve.kernel_size *
		elemSrc->data.Convolve.kernel_size) +
		(_XieTechniqueLength (xieValConvolve,
		elemSrc->data.Convolve.convolve_tech,
		elemSrc->data.Convolve.convolve_param) << 2);
	    break;

	case xieElemDither:
	    size += SIZEOF (xieFloDither) +
		(_XieTechniqueLength (xieValDither,
		elemSrc->data.Dither.dither_tech,
		elemSrc->data.Dither.dither_param) << 2);
	    break;

	case xieElemGeometry:
	    size += SIZEOF (xieFloGeometry) +
		(_XieTechniqueLength (xieValGeometry,
		elemSrc->data.Geometry.sample_tech,
		elemSrc->data.Geometry.sample_param) << 2);
	    break;

	case xieElemLogical:
	    size += SIZEOF (xieFloLogical);
	    break;

	case xieElemMatchHistogram:
	    size += SIZEOF (xieFloMatchHistogram) +
		(_XieTechniqueLength (xieValHistogram,
		elemSrc->data.MatchHistogram.shape,
		elemSrc->data.MatchHistogram.shape_param) << 2);
	    break;

	case xieElemMath:
	    size += SIZEOF (xieFloMath);
	    break;

	case xieElemPasteUp:
	    size += SIZEOF (xieFloPasteUp) +
		elemSrc->data.PasteUp.tile_count * SIZEOF (xieTypTile);
	    break;

	case xieElemPoint:
	    size += SIZEOF (xieFloPoint);
	    break;

	case xieElemUnconstrain:
	    size += SIZEOF (xieFloUnconstrain);
	    break;

	case xieElemExportClientHistogram:
	    size += SIZEOF (xieFloExportClientHistogram);
	    break;

	case xieElemExportClientLUT:
	    size += SIZEOF (xieFloExportClientLUT);
	    break;

	case xieElemExportClientPhoto:
	    size += SIZEOF (xieFloExportClientPhoto) +
		(_XieTechniqueLength (xieValEncode,
		elemSrc->data.ExportClientPhoto.encode_tech,
		elemSrc->data.ExportClientPhoto.encode_param) << 2);
	    break;

	case xieElemExportClientROI:
	    size += SIZEOF (xieFloExportClientROI);
	    break;

	case xieElemExportDrawable:
	    size += SIZEOF (xieFloExportDrawable);
	    break;

	case xieElemExportDrawablePlane:
	    size += SIZEOF (xieFloExportDrawablePlane);
	    break;

	case xieElemExportLUT:
	    size += SIZEOF (xieFloExportLUT);
	    break;

	case xieElemExportPhotomap:
	    size += SIZEOF (xieFloExportPhotomap) +
    		(_XieTechniqueLength (xieValEncode,
		elemSrc->data.ExportPhotomap.encode_tech,
		elemSrc->data.ExportPhotomap.encode_param) << 2);
	    break;

	case xieElemExportROI:
	    size += SIZEOF (xieFloExportROI);
	    break;

	default:
	    break;
	}
    }

    return (size);
}


void
_XieElemImportClientLUT (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloImportClientLUT	*elemDest;

    BEGIN_ELEM_HEAD (ImportClientLUT, elemSrc,
	LENOF (xieFloImportClientLUT), *bufDest, elemDest);

    elemDest->class     = elemSrc->data.ImportClientLUT.data_class;
    elemDest->bandOrder = elemSrc->data.ImportClientLUT.band_order;
    elemDest->length0	= elemSrc->data.ImportClientLUT.length[0];
    elemDest->length1	= elemSrc->data.ImportClientLUT.length[1];
    elemDest->length2	= elemSrc->data.ImportClientLUT.length[2];
    elemDest->levels0	= elemSrc->data.ImportClientLUT.levels[0];
    elemDest->levels1	= elemSrc->data.ImportClientLUT.levels[1];
    elemDest->levels2	= elemSrc->data.ImportClientLUT.levels[2];

    END_ELEM_HEAD (ImportClientLUT, *bufDest, elemDest);
}


void
_XieElemImportClientPhoto (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloImportClientPhoto	*elemDest;
    unsigned			techLen;

    techLen = _XieTechniqueLength (xieValDecode,
	elemSrc->data.ImportClientPhoto.decode_tech,
	elemSrc->data.ImportClientPhoto.decode_param);

    BEGIN_ELEM_HEAD (ImportClientPhoto, elemSrc,
	LENOF (xieFloImportClientPhoto) + techLen, *bufDest, elemDest);

    elemDest->notify		= elemSrc->data.ImportClientPhoto.notify;
    elemDest->class		= elemSrc->data.ImportClientPhoto.data_class;
    elemDest->width0		= elemSrc->data.ImportClientPhoto.width[0];
    elemDest->width1		= elemSrc->data.ImportClientPhoto.width[1];
    elemDest->width2		= elemSrc->data.ImportClientPhoto.width[2];
    elemDest->height0		= elemSrc->data.ImportClientPhoto.height[0];
    elemDest->height1		= elemSrc->data.ImportClientPhoto.height[1];
    elemDest->height2		= elemSrc->data.ImportClientPhoto.height[2];
    elemDest->levels0		= elemSrc->data.ImportClientPhoto.levels[0];
    elemDest->levels1		= elemSrc->data.ImportClientPhoto.levels[1];
    elemDest->levels2		= elemSrc->data.ImportClientPhoto.levels[2];
    elemDest->decodeTechnique	= elemSrc->data.ImportClientPhoto.decode_tech;
    elemDest->lenParams		= techLen;

    END_ELEM_HEAD (ImportClientPhoto, *bufDest, elemDest);

    /* Technique dependent decode params */

    _XieEncodeTechnique (bufDest, xieValDecode,
	elemSrc->data.ImportClientPhoto.decode_tech,
	elemSrc->data.ImportClientPhoto.decode_param);
}


void
_XieElemImportClientROI (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloImportClientROI	*elemDest;

    BEGIN_ELEM_HEAD (ImportClientROI, elemSrc,
	LENOF (xieFloImportClientROI), *bufDest, elemDest);

    elemDest->rectangles = elemSrc->data.ImportClientROI.rectangles;

    END_ELEM_HEAD (ImportClientROI, *bufDest, elemDest);
}


void
_XieElemImportDrawable (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloImportDrawable	*elemDest;

    BEGIN_ELEM_HEAD (ImportDrawable, elemSrc,
	LENOF (xieFloImportDrawable), *bufDest, elemDest);

    elemDest->drawable	= elemSrc->data.ImportDrawable.drawable;
    elemDest->srcX	= elemSrc->data.ImportDrawable.src_x;
    elemDest->srcY	= elemSrc->data.ImportDrawable.src_y;
    elemDest->width	= elemSrc->data.ImportDrawable.width;
    elemDest->height	= elemSrc->data.ImportDrawable.height;
    elemDest->fill	= elemSrc->data.ImportDrawable.fill;
    elemDest->notify	= elemSrc->data.ImportDrawable.notify;

    END_ELEM_HEAD (ImportDrawable, *bufDest, elemDest);
}


void
_XieElemImportDrawablePlane (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloImportDrawablePlane	*elemDest;

    BEGIN_ELEM_HEAD (ImportDrawablePlane, elemSrc,
	LENOF (xieFloImportDrawablePlane), *bufDest, elemDest);

    elemDest->drawable	= elemSrc->data.ImportDrawablePlane.drawable;
    elemDest->srcX	= elemSrc->data.ImportDrawablePlane.src_x;
    elemDest->srcY	= elemSrc->data.ImportDrawablePlane.src_y;
    elemDest->width	= elemSrc->data.ImportDrawablePlane.width;
    elemDest->height	= elemSrc->data.ImportDrawablePlane.height;
    elemDest->fill	= elemSrc->data.ImportDrawablePlane.fill;
    elemDest->bitPlane	= elemSrc->data.ImportDrawablePlane.bit_plane;
    elemDest->notify	= elemSrc->data.ImportDrawablePlane.notify;

    END_ELEM_HEAD (ImportDrawablePlane, *bufDest, elemDest);
}


void
_XieElemImportLUT (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloImportLUT	*elemDest;

    BEGIN_ELEM_HEAD (ImportLUT, elemSrc,
	LENOF (xieFloImportLUT), *bufDest, elemDest);

    elemDest->lut = elemSrc->data.ImportLUT.lut;

    END_ELEM_HEAD (ImportLUT, *bufDest, elemDest);
}


void
_XieElemImportPhotomap (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloImportPhotomap	*elemDest;

    BEGIN_ELEM_HEAD (ImportPhotomap, elemSrc,
	LENOF (xieFloImportPhotomap), *bufDest, elemDest);

    elemDest->photomap	= elemSrc->data.ImportPhotomap.photomap;
    elemDest->notify	= elemSrc->data.ImportPhotomap.notify;

    END_ELEM_HEAD (ImportPhotomap, *bufDest, elemDest);
}


void
_XieElemImportROI (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloImportROI	*elemDest;

    BEGIN_ELEM_HEAD (ImportROI, elemSrc,
	LENOF (xieFloImportROI), *bufDest, elemDest);

    elemDest->roi = elemSrc->data.ImportROI.roi;

    END_ELEM_HEAD (ImportROI, *bufDest, elemDest);
}


void
_XieElemArithmetic (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloArithmetic	*elemDest;

    BEGIN_ELEM_HEAD (Arithmetic, elemSrc,
	LENOF (xieFloArithmetic), *bufDest, elemDest);

    elemDest->src1		= elemSrc->data.Arithmetic.src1;
    elemDest->src2		= elemSrc->data.Arithmetic.src2;
    elemDest->domainOffsetX	= elemSrc->data.Arithmetic.domain.offset_x;
    elemDest->domainOffsetY	= elemSrc->data.Arithmetic.domain.offset_y;
    elemDest->domainPhototag	= elemSrc->data.Arithmetic.domain.phototag;
    elemDest->operator		= elemSrc->data.Arithmetic.operator;
    elemDest->bandMask		= elemSrc->data.Arithmetic.band_mask;
    elemDest->constant0	= 
	_XieConvertToIEEE (elemSrc->data.Arithmetic.constant[0]);
    elemDest->constant1	= 
	_XieConvertToIEEE (elemSrc->data.Arithmetic.constant[1]);
    elemDest->constant2	= 
	_XieConvertToIEEE (elemSrc->data.Arithmetic.constant[2]);

    END_ELEM_HEAD (Arithmetic, *bufDest, elemDest);
}


void
_XieElemBandCombine (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloBandCombine	*elemDest;

    BEGIN_ELEM_HEAD (BandCombine, elemSrc,
	LENOF (xieFloBandCombine), *bufDest, elemDest);

    elemDest->src1 = elemSrc->data.BandCombine.src1;
    elemDest->src2 = elemSrc->data.BandCombine.src2;
    elemDest->src3 = elemSrc->data.BandCombine.src3;

    END_ELEM_HEAD (BandCombine, *bufDest, elemDest);
}


void
_XieElemBandExtract (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloBandExtract	*elemDest;

    BEGIN_ELEM_HEAD (BandExtract, elemSrc,
	LENOF (xieFloBandExtract), *bufDest, elemDest);

    elemDest->src	= elemSrc->data.BandExtract.src;
    elemDest->levels	= elemSrc->data.BandExtract.levels;
    elemDest->bias	= 
	_XieConvertToIEEE (elemSrc->data.BandExtract.bias);
    elemDest->constant0	= 
	_XieConvertToIEEE (elemSrc->data.BandExtract.coefficients[0]);
    elemDest->constant1	= 
	_XieConvertToIEEE (elemSrc->data.BandExtract.coefficients[1]);
    elemDest->constant2	= 
	_XieConvertToIEEE (elemSrc->data.BandExtract.coefficients[2]);
    END_ELEM_HEAD (BandExtract, *bufDest, elemDest);
}

void
_XieElemBandSelect (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloBandSelect	*elemDest;

    BEGIN_ELEM_HEAD (BandSelect, elemSrc,
	LENOF (xieFloBandSelect), *bufDest, elemDest);

    elemDest->src	= elemSrc->data.BandSelect.src;
    elemDest->bandNumber= elemSrc->data.BandSelect.band_number;

    END_ELEM_HEAD (BandSelect, *bufDest, elemDest);
}


void
_XieElemBlend (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloBlend	*elemDest;

    BEGIN_ELEM_HEAD (Blend, elemSrc,
	LENOF (xieFloBlend), *bufDest, elemDest);

    elemDest->src1		= elemSrc->data.Blend.src1;
    elemDest->src2		= elemSrc->data.Blend.src2;
    elemDest->alpha		= elemSrc->data.Blend.alpha;
    elemDest->constant0	= 
	_XieConvertToIEEE (elemSrc->data.Blend.src_constant[0]);
    elemDest->constant1	= 
	_XieConvertToIEEE (elemSrc->data.Blend.src_constant[1]);
    elemDest->constant2	= 
	_XieConvertToIEEE (elemSrc->data.Blend.src_constant[2]);
    elemDest->alphaConst = 
	_XieConvertToIEEE (elemSrc->data.Blend.alpha_constant);
    elemDest->domainOffsetX 	= elemSrc->data.Blend.domain.offset_x;
    elemDest->domainOffsetY 	= elemSrc->data.Blend.domain.offset_y;
    elemDest->domainPhototag 	= elemSrc->data.Blend.domain.phototag;
    elemDest->bandMask		= elemSrc->data.Blend.band_mask;

    END_ELEM_HEAD (Blend, *bufDest, elemDest);
}


void
_XieElemCompare (bufDest, elemSrc)

char		**bufDest;
XiePhotoElement	*elemSrc;

{
    xieFloCompare	*elemDest;

    BEGIN_ELEM_HEAD (Compare, elemSrc,
	LENOF (xieFloCompare), *bufDest, elemDest);

    elemDest->src1		= elemSrc->data.Compare.src1;
    elemDest->src2		= elemSrc->data.Compare.src2;
    elemDest->domainOffsetX	= elemSrc->data.Compare.domain.offset_x;
    elemDest->domainOffsetY	= elemSrc->data.Compare.domain.offset_y;
    elemDest->domainPhototag	= elemSrc->data.Compare.domain.phototag;
    elemDest->operator		= elemSrc->data.Compare.operator;
    elemDest->combine		= elemSrc->data.Compare.combine;
    elemDest->constant0	= 
	_XieConvertToIEEE (elemSrc->data.Compare.constant[0]);
    elemDest->constant1	= 
	_XieConvertToIEEE (elemSrc->data.Compare.constant[1]);
    elemDest->constant2	= 
	_XieConvertToIEEE (elemSrc->data.Compare.constant[2]);
    elemDest->bandMask		= elemSrc->data.Compare.band_mask;

    END_ELEM_HEAD (Compare, *bufDest, elemDest);
}


void
_XieElemConstrain (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloConstrain	*elemDest;
    unsigned		techLen;

    techLen = _XieTechniqueLength (xieValConstrain,
	elemSrc->data.Constrain.constrain_tech,
	elemSrc->data.Constrain.constrain_param);

    BEGIN_ELEM_HEAD (Constrain, elemSrc,
	LENOF (xieFloConstrain) + techLen, *bufDest, elemDest);

    elemDest->src	= elemSrc->data.Constrain.src;
    elemDest->levels0	= elemSrc->data.Constrain.levels[0];
    elemDest->levels1 	= elemSrc->data.Constrain.levels[1];
    elemDest->levels2 	= elemSrc->data.Constrain.levels[2];
    elemDest->constrain = elemSrc->data.Constrain.constrain_tech;
    elemDest->lenParams = techLen;

    END_ELEM_HEAD (Constrain, *bufDest, elemDest);

    /* Technique dependent constrain params */

    _XieEncodeTechnique (bufDest, xieValConstrain,
	elemSrc->data.Constrain.constrain_tech,
	elemSrc->data.Constrain.constrain_param);
}


void
_XieElemConvertFromIndex (bufDest, elemSrc)

char		**bufDest;
XiePhotoElement	*elemSrc;

{
    xieFloConvertFromIndex	*elemDest;

    BEGIN_ELEM_HEAD (ConvertFromIndex, elemSrc,
	LENOF (xieFloConvertFromIndex), *bufDest, elemDest);

    elemDest->src 	= elemSrc->data.ConvertFromIndex.src;
    elemDest->class	= elemSrc->data.ConvertFromIndex.data_class;
    elemDest->precision = elemSrc->data.ConvertFromIndex.precision;
    elemDest->colormap 	= elemSrc->data.ConvertFromIndex.colormap;

    END_ELEM_HEAD (ConvertFromIndex, *bufDest, elemDest);
}


void
_XieElemConvertFromRGB (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloConvertFromRGB	*elemDest;
    unsigned			techLen;

    techLen = _XieTechniqueLength (xieValConvertFromRGB,
	elemSrc->data.ConvertFromRGB.color_space,
	elemSrc->data.ConvertFromRGB.color_param);

    BEGIN_ELEM_HEAD (ConvertFromRGB, elemSrc,
	LENOF (xieFloConvertFromRGB) + techLen, *bufDest, elemDest);

    elemDest->src 	= elemSrc->data.ConvertFromRGB.src;
    elemDest->convert 	= elemSrc->data.ConvertFromRGB.color_space;
    elemDest->lenParams	= techLen;

    END_ELEM_HEAD (ConvertFromRGB, *bufDest, elemDest);

    /* Technique dependent color params */

    _XieEncodeTechnique (bufDest, xieValConvertFromRGB,
	elemSrc->data.ConvertFromRGB.color_space,
	elemSrc->data.ConvertFromRGB.color_param);
}


void
_XieElemConvertToIndex (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloConvertToIndex	*elemDest;
    unsigned			techLen;

    techLen = _XieTechniqueLength (xieValColorAlloc,
	elemSrc->data.ConvertToIndex.color_alloc_tech,
	elemSrc->data.ConvertToIndex.color_alloc_param);

    BEGIN_ELEM_HEAD (ConvertToIndex, elemSrc,
	LENOF (xieFloConvertToIndex) + techLen, *bufDest, elemDest);

    elemDest->src 	 = elemSrc->data.ConvertToIndex.src;
    elemDest->notify 	 = elemSrc->data.ConvertToIndex.notify;
    elemDest->colormap 	 = elemSrc->data.ConvertToIndex.colormap;
    elemDest->colorList  = elemSrc->data.ConvertToIndex.color_list;
    elemDest->colorAlloc = elemSrc->data.ConvertToIndex.color_alloc_tech;
    elemDest->lenParams  = techLen;

    END_ELEM_HEAD (ConvertToIndex, *bufDest, elemDest);

    /* Technique dependent color alloc params */

    _XieEncodeTechnique (bufDest, xieValColorAlloc,
	elemSrc->data.ConvertToIndex.color_alloc_tech,
	elemSrc->data.ConvertToIndex.color_alloc_param);
}


void
_XieElemConvertToRGB (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloConvertToRGB	*elemDest;
    unsigned	        techLen;

    techLen = _XieTechniqueLength (xieValConvertToRGB,
	elemSrc->data.ConvertToRGB.color_space,
	elemSrc->data.ConvertToRGB.color_param);

    BEGIN_ELEM_HEAD (ConvertToRGB, elemSrc, 
	LENOF (xieFloConvertToRGB) + techLen, *bufDest, elemDest);

    elemDest->src 	= elemSrc->data.ConvertToRGB.src;
    elemDest->convert 	= elemSrc->data.ConvertToRGB.color_space;
    elemDest->lenParams = techLen;

    END_ELEM_HEAD (ConvertToRGB, *bufDest, elemDest);

    /* Technique dependent color params */

    _XieEncodeTechnique (bufDest, xieValConvertToRGB,
	elemSrc->data.ConvertToRGB.color_space,
	elemSrc->data.ConvertToRGB.color_param);
}


void
_XieElemConvolve (bufDest, elemSrc)

char		**bufDest;
XiePhotoElement	*elemSrc;

{
    int 		ksize = elemSrc->data.Convolve.kernel_size;
    int 		i, j;
    xieTypFloat		*fptr;
    xieFloConvolve	*elemDest;
    unsigned		techLen, kernelLen;

    techLen = _XieTechniqueLength (xieValConvolve,
	elemSrc->data.Convolve.convolve_tech,
	elemSrc->data.Convolve.convolve_param);
	
    kernelLen = elemSrc->data.Convolve.kernel_size *
	elemSrc->data.Convolve.kernel_size;

    BEGIN_ELEM_HEAD (Convolve, elemSrc, 
	LENOF (xieFloConvolve) + kernelLen + techLen, *bufDest, elemDest);

    elemDest->src 		= elemSrc->data.Convolve.src;
    elemDest->domainOffsetX 	= elemSrc->data.Convolve.domain.offset_x;
    elemDest->domainOffsetY 	= elemSrc->data.Convolve.domain.offset_y;
    elemDest->domainPhototag 	= elemSrc->data.Convolve.domain.phototag;
    elemDest->bandMask 		= elemSrc->data.Convolve.band_mask;
    elemDest->kernelSize 	= elemSrc->data.Convolve.kernel_size;
    elemDest->convolve 		= elemSrc->data.Convolve.convolve_tech;
    elemDest->lenParams 	= techLen;

    END_ELEM_HEAD (Convolve, *bufDest, elemDest);


    /* LISTofFloat (kernelSize^2) */

    fptr = (xieTypFloat *) *bufDest;
    for (i = 0; i < ksize; i++)
	for (j = 0; j < ksize; j++) 
	    *fptr++ = _XieConvertToIEEE (
		elemSrc->data.Convolve.kernel[i * ksize + j]);

    *bufDest += NUMBYTES (kernelLen);


    /* Technique dependent convolve params */

    _XieEncodeTechnique (bufDest, xieValConvolve,
	elemSrc->data.Convolve.convolve_tech,
	elemSrc->data.Convolve.convolve_param);

}


void
_XieElemDither (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloDither	*elemDest;
    unsigned		techLen;

    techLen = _XieTechniqueLength (xieValDither,
	elemSrc->data.Dither.dither_tech,
	elemSrc->data.Dither.dither_param);

    BEGIN_ELEM_HEAD (Dither, elemSrc,
	LENOF (xieFloDither) + techLen, *bufDest, elemDest);

    elemDest->src 	= elemSrc->data.Dither.src;
    elemDest->bandMask 	= elemSrc->data.Dither.band_mask;
    elemDest->levels0 	= elemSrc->data.Dither.levels[0];
    elemDest->levels1 	= elemSrc->data.Dither.levels[1];
    elemDest->levels2 	= elemSrc->data.Dither.levels[2];
    elemDest->dither 	= elemSrc->data.Dither.dither_tech;
    elemDest->lenParams = techLen;

    END_ELEM_HEAD (Dither, *bufDest, elemDest);

    /* Technique dependent dither params */

    _XieEncodeTechnique (bufDest, xieValDither,
	elemSrc->data.Dither.dither_tech,
	elemSrc->data.Dither.dither_param);
}


void
_XieElemGeometry (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloGeometry	*elemDest;
    unsigned		techLen;

    techLen = _XieTechniqueLength (xieValGeometry,
	elemSrc->data.Geometry.sample_tech,
	elemSrc->data.Geometry.sample_param);

    BEGIN_ELEM_HEAD (Geometry, elemSrc,
	LENOF (xieFloGeometry) + techLen, *bufDest, elemDest);

    elemDest->src	= elemSrc->data.Geometry.src;
    elemDest->bandMask	= elemSrc->data.Geometry.band_mask;
    elemDest->width 	= elemSrc->data.Geometry.width;
    elemDest->height 	= elemSrc->data.Geometry.height;
    elemDest->a 	= _XieConvertToIEEE (
		elemSrc->data.Geometry.coefficients[0]);
    elemDest->b 	= _XieConvertToIEEE (
		elemSrc->data.Geometry.coefficients[1]);
    elemDest->c 	= _XieConvertToIEEE (
		elemSrc->data.Geometry.coefficients[2]);
    elemDest->d 	= _XieConvertToIEEE (
		elemSrc->data.Geometry.coefficients[3]);
    elemDest->tx 	= _XieConvertToIEEE (
		elemSrc->data.Geometry.coefficients[4]);
    elemDest->ty 	= _XieConvertToIEEE (
		elemSrc->data.Geometry.coefficients[5]);
    elemDest->constant0 = _XieConvertToIEEE (
		elemSrc->data.Geometry.constant[0] );
    elemDest->constant1 = _XieConvertToIEEE (
		elemSrc->data.Geometry.constant[1] );
    elemDest->constant2 = _XieConvertToIEEE (
		elemSrc->data.Geometry.constant[2] );
    elemDest->sample 	= elemSrc->data.Geometry.sample_tech;
    elemDest->lenParams = techLen;

    END_ELEM_HEAD (Geometry, *bufDest, elemDest);

    /* Technique dependent sample params */

    _XieEncodeTechnique (bufDest, xieValGeometry,
	elemSrc->data.Geometry.sample_tech,
	elemSrc->data.Geometry.sample_param);
}


void
_XieElemLogical (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloLogical	*elemDest;

    BEGIN_ELEM_HEAD (Logical, elemSrc,
	LENOF (xieFloLogical), *bufDest, elemDest);

    elemDest->src1 		= elemSrc->data.Logical.src1;
    elemDest->src2 		= elemSrc->data.Logical.src2;
    elemDest->domainOffsetX 	= elemSrc->data.Logical.domain.offset_x;
    elemDest->domainOffsetY 	= elemSrc->data.Logical.domain.offset_y;
    elemDest->domainPhototag 	= elemSrc->data.Logical.domain.phototag;
    elemDest->operator 		= elemSrc->data.Logical.operator;
    elemDest->bandMask 		= elemSrc->data.Logical.band_mask;
    elemDest->constant0	= 
	_XieConvertToIEEE (elemSrc->data.Logical.constant[0]);
    elemDest->constant1	= 
	_XieConvertToIEEE (elemSrc->data.Logical.constant[1]);
    elemDest->constant2	= 
	_XieConvertToIEEE (elemSrc->data.Logical.constant[2]);

    END_ELEM_HEAD (Logical, *bufDest, elemDest);
}


void
_XieElemMatchHistogram (bufDest, elemSrc)

char		**bufDest;
XiePhotoElement	*elemSrc;

{
    xieFloMatchHistogram	*elemDest;
    unsigned			techLen;

    techLen = _XieTechniqueLength (xieValHistogram,
	elemSrc->data.MatchHistogram.shape,
	elemSrc->data.MatchHistogram.shape_param);

    BEGIN_ELEM_HEAD (MatchHistogram, elemSrc,
	LENOF (xieFloMatchHistogram) + techLen, *bufDest, elemDest);

    elemDest->src 	     = elemSrc->data.MatchHistogram.src;
    elemDest->domainOffsetX  = elemSrc->data.MatchHistogram.domain.offset_x;
    elemDest->domainOffsetY  = elemSrc->data.MatchHistogram.domain.offset_y;
    elemDest->domainPhototag = elemSrc->data.MatchHistogram.domain.phototag;
    elemDest->shape 	     = elemSrc->data.MatchHistogram.shape;
    elemDest->lenParams      = techLen;

    END_ELEM_HEAD (MatchHistogram, *bufDest, elemDest);

    /* Technique dependent shape params */

    _XieEncodeTechnique (bufDest, xieValHistogram,
	elemSrc->data.MatchHistogram.shape,
	elemSrc->data.MatchHistogram.shape_param);
}


void
_XieElemMath (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloMath	*elemDest;

    BEGIN_ELEM_HEAD (Math, elemSrc,
	LENOF (xieFloMath), *bufDest, elemDest);

    elemDest->src 		= elemSrc->data.Math.src;
    elemDest->domainOffsetX 	= elemSrc->data.Math.domain.offset_x;
    elemDest->domainOffsetY 	= elemSrc->data.Math.domain.offset_y;
    elemDest->domainPhototag 	= elemSrc->data.Math.domain.phototag;
    elemDest->operator 		= elemSrc->data.Math.operator;
    elemDest->bandMask 		= elemSrc->data.Math.band_mask;

    END_ELEM_HEAD (Math, *bufDest, elemDest);
}


void
_XieElemPasteUp (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloPasteUp	*elemDest;

    BEGIN_ELEM_HEAD (PasteUp, elemSrc,
	LENOF (xieFloPasteUp) +
	elemSrc->data.PasteUp.tile_count * LENOF (xieTypTile),
	*bufDest, elemDest);

    elemDest->numTiles 	= elemSrc->data.PasteUp.tile_count;
    elemDest->width 	= elemSrc->data.PasteUp.width;
    elemDest->height 	= elemSrc->data.PasteUp.height;
    elemDest->constant0	= 
	_XieConvertToIEEE (elemSrc->data.PasteUp.constant[0]);
    elemDest->constant1	= 
	_XieConvertToIEEE (elemSrc->data.PasteUp.constant[1]);
    elemDest->constant2	= 
	_XieConvertToIEEE (elemSrc->data.PasteUp.constant[2]);

    END_ELEM_HEAD (PasteUp, *bufDest, elemDest);

    /* LISTofTile (numTiles) */

    STORE_LISTOF_TILES (elemSrc->data.PasteUp.tiles,
	elemSrc->data.PasteUp.tile_count, *bufDest);
}


void
_XieElemPoint (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloPoint	*elemDest;

    BEGIN_ELEM_HEAD (Point, elemSrc,
	LENOF (xieFloPoint), *bufDest, elemDest);

    elemDest->src 		= elemSrc->data.Point.src;
    elemDest->lut 		= elemSrc->data.Point.lut;
    elemDest->domainOffsetX 	= elemSrc->data.Point.domain.offset_x;
    elemDest->domainOffsetY 	= elemSrc->data.Point.domain.offset_y;
    elemDest->domainPhototag 	= elemSrc->data.Point.domain.phototag;
    elemDest->bandMask 		= elemSrc->data.Point.band_mask;

    END_ELEM_HEAD (Point, *bufDest, elemDest);
}

void 
_XieElemUnconstrain (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloUnconstrain	*elemDest;

    BEGIN_ELEM_HEAD (Unconstrain, elemSrc,
	LENOF (xieFloUnconstrain), *bufDest, elemDest);

    elemDest->src 		= elemSrc->data.Unconstrain.src;

    END_ELEM_HEAD (Unconstrain, *bufDest, elemDest);
}

void
_XieElemExportClientHistogram (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloExportClientHistogram	*elemDest;

    BEGIN_ELEM_HEAD (ExportClientHistogram, elemSrc,
	LENOF (xieFloExportClientHistogram), *bufDest, elemDest);

    elemDest->src 	     = elemSrc->data.ExportClientHistogram.src;
    elemDest->notify 	     = elemSrc->data.ExportClientHistogram.notify;
    elemDest->domainOffsetX  =
	elemSrc->data.ExportClientHistogram.domain.offset_x;
    elemDest->domainOffsetY  =
	elemSrc->data.ExportClientHistogram.domain.offset_y;
    elemDest->domainPhototag =
	elemSrc->data.ExportClientHistogram.domain.phototag;

    END_ELEM_HEAD (ExportClientHistogram, *bufDest, elemDest);
}


void
_XieElemExportClientLUT (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloExportClientLUT	*elemDest;

    BEGIN_ELEM_HEAD (ExportClientLUT, elemSrc,
	LENOF (xieFloExportClientLUT), *bufDest, elemDest);

    elemDest->src 	= elemSrc->data.ExportClientLUT.src;
    elemDest->notify 	= elemSrc->data.ExportClientLUT.notify;
    elemDest->bandOrder = elemSrc->data.ExportClientLUT.band_order;
    elemDest->start0    = elemSrc->data.ExportClientLUT.start[0];
    elemDest->start1    = elemSrc->data.ExportClientLUT.start[1];
    elemDest->start2    = elemSrc->data.ExportClientLUT.start[2];
    elemDest->length0   = elemSrc->data.ExportClientLUT.length[0];
    elemDest->length1   = elemSrc->data.ExportClientLUT.length[1];
    elemDest->length2   = elemSrc->data.ExportClientLUT.length[2];

    END_ELEM_HEAD (ExportClientLUT, *bufDest, elemDest);
}


void
_XieElemExportClientPhoto (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloExportClientPhoto	*elemDest;
    unsigned			techLen;

    techLen = _XieTechniqueLength (xieValEncode,
	elemSrc->data.ExportClientPhoto.encode_tech,
	elemSrc->data.ExportClientPhoto.encode_param);
	
    BEGIN_ELEM_HEAD (ExportClientPhoto, elemSrc,
	LENOF (xieFloExportClientPhoto) + techLen, *bufDest, elemDest);

    elemDest->src 		= elemSrc->data.ExportClientPhoto.src;
    elemDest->notify 		= elemSrc->data.ExportClientPhoto.notify;
    elemDest->encodeTechnique 	= elemSrc->data.ExportClientPhoto.encode_tech;
    elemDest->lenParams 	= techLen;

    END_ELEM_HEAD (ExportClientPhoto, *bufDest, elemDest);

    /* Technique dependent encode params */

    _XieEncodeTechnique (bufDest, xieValEncode,
	elemSrc->data.ExportClientPhoto.encode_tech,
	elemSrc->data.ExportClientPhoto.encode_param);
}


void
_XieElemExportClientROI (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloExportClientROI	*elemDest;

    BEGIN_ELEM_HEAD (ExportClientROI, elemSrc,
	LENOF (xieFloExportClientROI), *bufDest, elemDest);

    elemDest->src 	= elemSrc->data.ExportClientROI.src;
    elemDest->notify 	= elemSrc->data.ExportClientROI.notify;

    END_ELEM_HEAD (ExportClientROI, *bufDest, elemDest);
}


void
_XieElemExportDrawable (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloExportDrawable	*elemDest;

    BEGIN_ELEM_HEAD (ExportDrawable, elemSrc,
	LENOF (xieFloExportDrawable), *bufDest, elemDest);

    elemDest->src 	= elemSrc->data.ExportDrawable.src;
    elemDest->dstX 	= elemSrc->data.ExportDrawable.dst_x;
    elemDest->dstY 	= elemSrc->data.ExportDrawable.dst_y;
    elemDest->drawable 	= elemSrc->data.ExportDrawable.drawable;
    elemDest->gc 	= (elemSrc->data.ExportDrawable.gc)->gid;

    END_ELEM_HEAD (ExportDrawable, *bufDest, elemDest);
}


void
_XieElemExportDrawablePlane (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloExportDrawablePlane	*elemDest;

    BEGIN_ELEM_HEAD (ExportDrawablePlane, elemSrc,
	LENOF (xieFloExportDrawablePlane), *bufDest, elemDest);

    elemDest->src 	= elemSrc->data.ExportDrawablePlane.src;
    elemDest->dstX 	= elemSrc->data.ExportDrawablePlane.dst_x;
    elemDest->dstY 	= elemSrc->data.ExportDrawablePlane.dst_y;
    elemDest->drawable 	= elemSrc->data.ExportDrawablePlane.drawable;
    elemDest->gc 	= (elemSrc->data.ExportDrawablePlane.gc)->gid;

    END_ELEM_HEAD (ExportDrawablePlane, *bufDest, elemDest);
}


void
_XieElemExportLUT (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloExportLUT	*elemDest;

    BEGIN_ELEM_HEAD (ExportLUT, elemSrc,
	LENOF (xieFloExportLUT), *bufDest, elemDest);

    elemDest->src    = elemSrc->data.ExportLUT.src;
    elemDest->lut    = elemSrc->data.ExportLUT.lut;
    elemDest->merge  = elemSrc->data.ExportLUT.merge;
    elemDest->start0 = elemSrc->data.ExportLUT.start[0];
    elemDest->start1 = elemSrc->data.ExportLUT.start[1];
    elemDest->start2 = elemSrc->data.ExportLUT.start[2];

    END_ELEM_HEAD (ExportLUT, *bufDest, elemDest);
}


void
_XieElemExportPhotomap (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloExportPhotomap	*elemDest;
    unsigned			techLen;

    techLen = _XieTechniqueLength (xieValEncode,
	elemSrc->data.ExportPhotomap.encode_tech,
	elemSrc->data.ExportPhotomap.encode_param);
	
    BEGIN_ELEM_HEAD (ExportPhotomap, elemSrc,
	LENOF (xieFloExportPhotomap) + techLen, *bufDest, elemDest);

    elemDest->src 		= elemSrc->data.ExportPhotomap.src;
    elemDest->photomap 		= elemSrc->data.ExportPhotomap.photomap;
    elemDest->encodeTechnique 	= elemSrc->data.ExportPhotomap.encode_tech;
    elemDest->lenParams 	= techLen;

    END_ELEM_HEAD (ExportPhotomap, *bufDest, elemDest);

    /* Technique dependent encode params */

    _XieEncodeTechnique (bufDest, xieValEncode,
	elemSrc->data.ExportPhotomap.encode_tech,
	elemSrc->data.ExportPhotomap.encode_param);
}


void
_XieElemExportROI (
	char		**bufDest,
	XiePhotoElement	*elemSrc)
{
    xieFloExportROI	*elemDest;

    BEGIN_ELEM_HEAD (ExportROI, elemSrc,
	LENOF (xieFloExportROI), *bufDest, elemDest);

    elemDest->src = elemSrc->data.ExportROI.src;
    elemDest->roi = elemSrc->data.ExportROI.roi;

    END_ELEM_HEAD (ExportROI, *bufDest, elemDest);
}
