/* $Xorg: technique.c,v 1.4 2001/02/09 02:03:41 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/XIE/technique.c,v 1.5 2001/12/14 19:54:34 dawes Exp $ */

#include "XIElibint.h"
#include "technique.h"

/* these really should have been static, but they're listed in XIEos2.def */

extern int _XieColorAllocAllParam		(char **, XiePointer, int);
extern int _XieColorAllocMatchParam		(char **, XiePointer, int);
extern int _XieColorAllocRequantizeParam	(char **, XiePointer, int);
extern int _XieRGBToCIELabParam			(char **, XiePointer, int);
extern int _XieRGBToYCbCrParam			(char **, XiePointer, int);
extern int _XieRGBToYCCParam			(char **, XiePointer, int);
extern int _XieYCbCrToRGBParam			(char **, XiePointer, int);
extern int _XieYCCToRGBParam			(char **, XiePointer, int);
extern int _XieCIELabToRGBParam			(char **, XiePointer, int);
extern int _XieConvolveConstantParam		(char **, XiePointer, int);
extern int _XieClipScaleParam			(char **, XiePointer, int);
extern int _XieDecodeUncompressedSingleParam	(char **, XiePointer, int);
extern int _XieDecodeUncompressedTripleParam	(char **, XiePointer, int);
extern int _XieDecodeG31DParam			(char **, XiePointer, int);
extern int _XieDecodeTIFFPackBitsParam		(char **, XiePointer, int);
extern int _XieDecodeJPEGBaselineParam		(char **, XiePointer, int);
extern int _XieDitherOrderedParam		(char **, XiePointer, int);
extern int _XieEncodeUncompressedSingleParam	(char **, XiePointer, int);
extern int _XieEncodeUncompressedTripleParam	(char **, XiePointer, int);
extern int _XieEncodeG31DParam			(char **, XiePointer, int);
extern int _XieEncodeTIFFPackBitsParam		(char **, XiePointer, int);
extern int _XieEncodeServerChoiceParam		(char **, XiePointer, int);
extern int _XieEncodeG32DParam			(char **, XiePointer, int);
extern int _XieEncodeG42DParam			(char **, XiePointer, int);
extern int _XieEncodeJPEGBaselineParam		(char **, XiePointer, int);
extern int _XieEncodeJPEGLosslessParam		(char **, XiePointer, int);
extern int _XieEncodeTIFF2Param			(char **, XiePointer, int);
extern int _XieGeomAntialiasByAreaParam		(char **, XiePointer, int);
extern int _XieGeomAntialiasByLowpassParam	(char **, XiePointer, int);
extern int _XieGeomGaussianParam		(char **, XiePointer, int);
extern int _XieGeomNearestNeighborParam		(char **, XiePointer, int);
extern int _XieHistogramGaussianParam		(char **, XiePointer, int);
extern int _XieHistogramHyperbolicParam		(char **, XiePointer, int);
extern int _XieWhiteAdjustCIELabShiftParam	(char **, XiePointer, int);


int
_XieTechniqueLength (int group, int technique, XiePointer param)
{
    XieTechFuncPtr techfunc = _XieLookupTechFunc (group, technique);
    int length;

    if (techfunc)
	length = (*techfunc) (NULL, param, 0);
    else
	length = 0;

    return (length);
}


void
_XieEncodeTechnique (
	char **bufDest,
	int  group,
	int  technique,
	XiePointer param)
{
    XieTechFuncPtr techfunc = _XieLookupTechFunc (group, technique);

    if (techfunc)
	(void) (*techfunc) (bufDest, param, 1);
}



#define srcParam ((XieColorAllocAllParam *)srcPtr)
int
_XieColorAllocAllParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (CARD32);

    if (srcParam == NULL)
	return(0);

    if (encode)
	STORE_CARD32 (srcParam->fill, *bufDest);

    return (length);
}
#undef srcParam


#define srcParam ((XieColorAllocMatchParam *)srcPtr)
int
_XieColorAllocMatchParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecColorAllocMatch);
    xieTecColorAllocMatch *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecColorAllocMatch, *bufDest, dstParam);

	dstParam->matchLimit = _XieConvertToIEEE (srcParam->match_limit);
	dstParam->grayLimit = _XieConvertToIEEE (srcParam->gray_limit);

	END_TECHNIQUE (xieTecColorAllocMatch, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieColorAllocRequantizeParam	*)srcPtr)
int
_XieColorAllocRequantizeParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (CARD32);

    if (srcParam == NULL)
	return(0);

    if (encode)
	STORE_CARD32 (srcParam->max_cells, *bufDest);

    return (length);
}
#undef srcParam


#define srcParam ((XieRGBToCIELabParam *)srcPtr)
int
_XieRGBToCIELabParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length, techLen;
    xieTecRGBToCIELab *dstParam;

    if (srcParam == NULL)
	return(0);

    techLen = _XieTechniqueLength (xieValWhiteAdjust,
	srcParam->white_adjust_tech, srcParam->white_adjust_param);

    length = LENOF (xieTecRGBToCIELab) + techLen;

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecRGBToCIELab, *bufDest, dstParam);

	/* memcpy (dstParam->matrix00, srcParam->matrix, 36); */
        dstParam->matrix00 = _XieConvertToIEEE (srcParam->matrix[0]);
        dstParam->matrix01 = _XieConvertToIEEE (srcParam->matrix[1]);
        dstParam->matrix02 = _XieConvertToIEEE (srcParam->matrix[2]);
        dstParam->matrix10 = _XieConvertToIEEE (srcParam->matrix[3]);
        dstParam->matrix11 = _XieConvertToIEEE (srcParam->matrix[4]);
        dstParam->matrix12 = _XieConvertToIEEE (srcParam->matrix[5]);
        dstParam->matrix20 = _XieConvertToIEEE (srcParam->matrix[6]);
        dstParam->matrix21 = _XieConvertToIEEE (srcParam->matrix[7]);
        dstParam->matrix22 = _XieConvertToIEEE (srcParam->matrix[8]);

	dstParam->whiteAdjusted = srcParam->white_adjust_tech;
	dstParam->lenParams = techLen;

	END_TECHNIQUE (xieTecRGBToCIELab, *bufDest, dstParam);

	_XieEncodeTechnique (bufDest, xieValWhiteAdjust,
	    srcParam->white_adjust_tech, srcParam->white_adjust_param);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieRGBToYCbCrParam *)srcPtr)
int
_XieRGBToYCbCrParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecRGBToYCbCr);
    xieTecRGBToYCbCr *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecRGBToYCbCr, *bufDest, dstParam);

	dstParam->levels0   = srcParam->levels[0];
	dstParam->levels1   = srcParam->levels[1];
	dstParam->levels2   = srcParam->levels[2];
        dstParam->lumaRed   = _XieConvertToIEEE (srcParam->luma_red);
        dstParam->lumaGreen = _XieConvertToIEEE (srcParam->luma_green);
        dstParam->lumaBlue  = _XieConvertToIEEE (srcParam->luma_blue);
        dstParam->bias0     = _XieConvertToIEEE (srcParam->bias[0]);
        dstParam->bias1     = _XieConvertToIEEE (srcParam->bias[1]);
        dstParam->bias2     = _XieConvertToIEEE (srcParam->bias[2]);

	END_TECHNIQUE (xieTecRGBToYCbCr, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieRGBToYCCParam *)srcPtr)
int
_XieRGBToYCCParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecRGBToYCC);
    xieTecRGBToYCC *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecRGBToYCC, *bufDest, dstParam);

	dstParam->levels0   = srcParam->levels[0];
	dstParam->levels1   = srcParam->levels[1];
	dstParam->levels2   = srcParam->levels[2];
        dstParam->lumaRed   = _XieConvertToIEEE (srcParam->luma_red);
        dstParam->lumaGreen = _XieConvertToIEEE (srcParam->luma_green);
        dstParam->lumaBlue  = _XieConvertToIEEE (srcParam->luma_blue);
        dstParam->scale     = _XieConvertToIEEE (srcParam->scale);

	END_TECHNIQUE (xieTecRGBToYCC, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieYCbCrToRGBParam	*)srcPtr)
int
_XieYCbCrToRGBParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length, gamutLen;
    xieTecYCbCrToRGB *dstParam;

    if (srcParam == NULL)
	return(0);

    gamutLen = _XieTechniqueLength (xieValGamut,
	srcParam->gamut_tech, srcParam->gamut_param);

    length = LENOF (xieTecYCbCrToRGB) + gamutLen;

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecYCbCrToRGB, *bufDest, dstParam);

	dstParam->levels0   = srcParam->levels[0];
	dstParam->levels1   = srcParam->levels[1];
	dstParam->levels2   = srcParam->levels[2];
        dstParam->lumaRed   = _XieConvertToIEEE (srcParam->luma_red);
        dstParam->lumaGreen = _XieConvertToIEEE (srcParam->luma_green);
        dstParam->lumaBlue  = _XieConvertToIEEE (srcParam->luma_blue);
        dstParam->bias0     = _XieConvertToIEEE (srcParam->bias[0]);
        dstParam->bias1     = _XieConvertToIEEE (srcParam->bias[1]);
        dstParam->bias2     = _XieConvertToIEEE (srcParam->bias[2]);
	dstParam->gamutCompress  = srcParam->gamut_tech;
	dstParam->lenGamutParams = gamutLen;

	END_TECHNIQUE (xieTecYCbCrToRGB, *bufDest, dstParam);

	_XieEncodeTechnique (bufDest, xieValGamut,
	    srcParam->gamut_tech, srcParam->gamut_param);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieYCCToRGBParam *)srcPtr)
int
_XieYCCToRGBParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length, gamutLen;
    xieTecYCCToRGB *dstParam;

    if (srcParam == NULL)
	return(0);

    gamutLen = _XieTechniqueLength (xieValGamut,
	srcParam->gamut_tech, srcParam->gamut_param);

    length = LENOF (xieTecYCCToRGB) + gamutLen;

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecYCCToRGB, *bufDest, dstParam);

	dstParam->levels0   = srcParam->levels[0];
	dstParam->levels1   = srcParam->levels[1];
	dstParam->levels2   = srcParam->levels[2];
        dstParam->lumaRed   = _XieConvertToIEEE (srcParam->luma_red);
        dstParam->lumaGreen = _XieConvertToIEEE (srcParam->luma_green);
        dstParam->lumaBlue  = _XieConvertToIEEE (srcParam->luma_blue);
        dstParam->scale     = _XieConvertToIEEE (srcParam->scale);
	dstParam->gamutCompress  = srcParam->gamut_tech;
	dstParam->lenGamutParams = gamutLen;

	END_TECHNIQUE (xieTecYCCToRGB, *bufDest, dstParam);

	_XieEncodeTechnique (bufDest, xieValGamut,
	    srcParam->gamut_tech, srcParam->gamut_param);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieCIELabToRGBParam	*)srcPtr)
int
_XieCIELabToRGBParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    xieTecCIELabToRGB *dstParam;
    int length, whiteLen, gamutLen;

    if (srcParam == NULL)
	return(0);

    whiteLen = _XieTechniqueLength (xieValWhiteAdjust,
	srcParam->white_adjust_tech, srcParam->white_adjust_param);

    gamutLen = _XieTechniqueLength (xieValGamut,
	srcParam->gamut_tech, srcParam->gamut_param);

    length = LENOF (xieTecCIELabToRGB) + whiteLen + gamutLen;

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecCIELabToRGB, *bufDest, dstParam);

	/* memcpy (dstParam->matrix00, srcParam->matrix, 36); */
        dstParam->matrix00 = _XieConvertToIEEE (srcParam->matrix[0]);
        dstParam->matrix01 = _XieConvertToIEEE (srcParam->matrix[1]);
        dstParam->matrix02 = _XieConvertToIEEE (srcParam->matrix[2]);
        dstParam->matrix10 = _XieConvertToIEEE (srcParam->matrix[3]);
        dstParam->matrix11 = _XieConvertToIEEE (srcParam->matrix[4]);
        dstParam->matrix12 = _XieConvertToIEEE (srcParam->matrix[5]);
        dstParam->matrix20 = _XieConvertToIEEE (srcParam->matrix[6]);
        dstParam->matrix21 = _XieConvertToIEEE (srcParam->matrix[7]);
        dstParam->matrix22 = _XieConvertToIEEE (srcParam->matrix[8]);

	dstParam->whiteAdjusted  = srcParam->white_adjust_tech;
	dstParam->lenWhiteParams = whiteLen;
	dstParam->gamutCompress  = srcParam->gamut_tech;
	dstParam->lenGamutParams = gamutLen;

	END_TECHNIQUE (xieTecCIELabToRGB, *bufDest, dstParam);

	_XieEncodeTechnique (bufDest, xieValWhiteAdjust,
	    srcParam->white_adjust_tech, srcParam->white_adjust_param);

	_XieEncodeTechnique (bufDest, xieValGamut,
	    srcParam->gamut_tech, srcParam->gamut_param);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieConvolveConstantParam *)srcPtr)
int
_XieConvolveConstantParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    xieTecConvolveConstant 	*dstParam;
    int 			length = LENOF (xieTecConvolveConstant);

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecConvolveConstant, *bufDest, dstParam);

        dstParam->constant0 = _XieConvertToIEEE (srcParam->constant[0]);
        dstParam->constant1 = _XieConvertToIEEE (srcParam->constant[1]);
        dstParam->constant2 = _XieConvertToIEEE (srcParam->constant[2]);

	END_TECHNIQUE (xieTecConvolveConstant, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieClipScaleParam	*)srcPtr)
int
_XieClipScaleParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    xieTecClipScale 	*dstParam;
    int 		length = LENOF (xieTecClipScale);

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecClipScale, *bufDest, dstParam);

	dstParam->inputLow0   = _XieConvertToIEEE (srcParam->input_low[0]);
	dstParam->inputLow1   = _XieConvertToIEEE (srcParam->input_low[1]);
	dstParam->inputLow2   = _XieConvertToIEEE (srcParam->input_low[2]);
	dstParam->inputHigh0  = _XieConvertToIEEE (srcParam->input_high[0]);
	dstParam->inputHigh1  = _XieConvertToIEEE (srcParam->input_high[1]);
	dstParam->inputHigh2  = _XieConvertToIEEE (srcParam->input_high[2]);
	dstParam->outputLow0  = srcParam->output_low[0];
	dstParam->outputLow1  = srcParam->output_low[1];
	dstParam->outputLow2  = srcParam->output_low[2];
	dstParam->outputHigh0 = srcParam->output_high[0];
	dstParam->outputHigh1 = srcParam->output_high[1];
	dstParam->outputHigh2 = srcParam->output_high[2];

	END_TECHNIQUE (xieTecClipScale, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieDecodeUncompressedSingleParam *)srcPtr)
int
_XieDecodeUncompressedSingleParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecDecodeUncompressedSingle);
    xieTecDecodeUncompressedSingle *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecDecodeUncompressedSingle, *bufDest, dstParam);

	dstParam->fillOrder = srcParam->fill_order;
	dstParam->pixelOrder = srcParam->pixel_order;
	dstParam->pixelStride = srcParam->pixel_stride;
	dstParam->leftPad = srcParam->left_pad;
	dstParam->scanlinePad = srcParam->scanline_pad;

	END_TECHNIQUE (xieTecDecodeUncompressedSingle, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieDecodeUncompressedTripleParam *)srcPtr)
int
_XieDecodeUncompressedTripleParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecDecodeUncompressedTriple);
    xieTecDecodeUncompressedTriple *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecDecodeUncompressedTriple, *bufDest, dstParam);

	dstParam->leftPad[0] = srcParam->left_pad[0];
	dstParam->leftPad[1] = srcParam->left_pad[1];
	dstParam->leftPad[2] = srcParam->left_pad[2];
	dstParam->fillOrder = srcParam->fill_order;
	dstParam->pixelStride[0] = srcParam->pixel_stride[0];
	dstParam->pixelStride[1] = srcParam->pixel_stride[1];
	dstParam->pixelStride[2] = srcParam->pixel_stride[2];
	dstParam->pixelOrder = srcParam->pixel_order;
	dstParam->scanlinePad[0] = srcParam->scanline_pad[0];
	dstParam->scanlinePad[1] = srcParam->scanline_pad[1];
	dstParam->scanlinePad[2] = srcParam->scanline_pad[2];
	dstParam->bandOrder = srcParam->band_order;
	dstParam->interleave = srcParam->interleave;

	END_TECHNIQUE (xieTecDecodeUncompressedTriple, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieDecodeG31DParam *)srcPtr)
int
_XieDecodeG31DParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecDecodeG31D);
    xieTecDecodeG31D *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecDecodeG31D, *bufDest, dstParam);

	dstParam->encodedOrder = srcParam->encoded_order;
	dstParam->normal = srcParam->normal;
	dstParam->radiometric = srcParam->radiometric;

	END_TECHNIQUE (xieTecDecodeG31D, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieDecodeTIFFPackBitsParam *)srcPtr)
int
_XieDecodeTIFFPackBitsParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecDecodeTIFFPackBits);
    xieTecDecodeTIFFPackBits *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecDecodeTIFFPackBits, *bufDest, dstParam);

	dstParam->encodedOrder = srcParam->encoded_order;
	dstParam->normal = srcParam->normal;

	END_TECHNIQUE (xieTecDecodeTIFFPackBits, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieDecodeJPEGBaselineParam *)srcPtr)
int
_XieDecodeJPEGBaselineParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    xieTecDecodeJPEGBaseline *dstParam;
    int length = LENOF (xieTecDecodeJPEGBaseline);

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecDecodeJPEGBaseline, *bufDest, dstParam);

	dstParam->interleave = srcParam->interleave;
	dstParam->bandOrder = srcParam->band_order;
	dstParam->upSample = srcParam->up_sample;

	END_TECHNIQUE (xieTecDecodeJPEGBaseline, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieDitherOrderedParam *)srcPtr)
int
_XieDitherOrderedParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecDitherOrdered);
    xieTecDitherOrdered *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecDitherOrdered, *bufDest, dstParam);

	dstParam->thresholdOrder = srcParam->threshold_order;

	END_TECHNIQUE (xieTecDitherOrdered, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieEncodeUncompressedSingleParam *)srcPtr)
int
_XieEncodeUncompressedSingleParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecEncodeUncompressedSingle);
    xieTecEncodeUncompressedSingle *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecEncodeUncompressedSingle, *bufDest, dstParam);

	dstParam->fillOrder = srcParam->fill_order;
	dstParam->pixelOrder = srcParam->pixel_order;
	dstParam->pixelStride = srcParam->pixel_stride;
	dstParam->scanlinePad = srcParam->scanline_pad;

	END_TECHNIQUE (xieTecEncodeUncompressedSingle, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieEncodeUncompressedTripleParam *)srcPtr)
int
_XieEncodeUncompressedTripleParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecEncodeUncompressedTriple);
    xieTecEncodeUncompressedTriple *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecEncodeUncompressedTriple, *bufDest, dstParam);

	dstParam->pixelStride[0] = srcParam->pixel_stride[0];
	dstParam->pixelStride[1] = srcParam->pixel_stride[1];
	dstParam->pixelStride[2] = srcParam->pixel_stride[2];
	dstParam->pixelOrder = srcParam->pixel_order;
	dstParam->scanlinePad[0] = srcParam->scanline_pad[0];
	dstParam->scanlinePad[1] = srcParam->scanline_pad[1];
	dstParam->scanlinePad[2] = srcParam->scanline_pad[2];
	dstParam->fillOrder = srcParam->fill_order;
	dstParam->bandOrder = srcParam->band_order;
	dstParam->interleave = srcParam->interleave;

	END_TECHNIQUE (xieTecEncodeUncompressedTriple, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieEncodeG31DParam *)srcPtr)
int
_XieEncodeG31DParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecEncodeG31D);
    xieTecEncodeG31D *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecEncodeG31D, *bufDest, dstParam);

	dstParam->alignEol = srcParam->align_eol;
	dstParam->encodedOrder = srcParam->encoded_order;
	dstParam->radiometric = srcParam->radiometric;

	END_TECHNIQUE (xieTecEncodeG31D, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieEncodeTIFFPackBitsParam *)srcPtr)
int
_XieEncodeTIFFPackBitsParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecEncodeTIFFPackBits);
    xieTecEncodeTIFFPackBits *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecEncodeTIFFPackBits, *bufDest, dstParam);

	dstParam->encodedOrder = srcParam->encoded_order;

	END_TECHNIQUE (xieTecEncodeTIFFPackBits, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieEncodeServerChoiceParam *)srcPtr)
int
_XieEncodeServerChoiceParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecEncodeServerChoice);
    xieTecEncodeServerChoice *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecEncodeServerChoice, *bufDest, dstParam);

	dstParam->preference = srcParam->preference;

	END_TECHNIQUE (xieTecEncodeServerChoice, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieEncodeG32DParam *)srcPtr)
int
_XieEncodeG32DParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecEncodeG32D);
    xieTecEncodeG32D *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecEncodeG32D, *bufDest, dstParam);

	dstParam->uncompressed = srcParam->uncompressed;
	dstParam->alignEol = srcParam->align_eol;
	dstParam->encodedOrder = srcParam->encoded_order;
	dstParam->kFactor = srcParam->k_factor;
	dstParam->radiometric = srcParam->radiometric;

	END_TECHNIQUE (xieTecEncodeG32D, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieEncodeG42DParam *)srcPtr)
int
_XieEncodeG42DParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecEncodeG42D);
    xieTecEncodeG42D *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecEncodeG42D, *bufDest, dstParam);

	dstParam->uncompressed = srcParam->uncompressed;
	dstParam->encodedOrder = srcParam->encoded_order;
	dstParam->radiometric = srcParam->radiometric;

	END_TECHNIQUE (xieTecEncodeG42D, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieEncodeJPEGBaselineParam *)srcPtr)
int
_XieEncodeJPEGBaselineParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecEncodeJPEGBaseline) +
	((srcParam->q_size + srcParam->ac_size + srcParam->dc_size) >> 2);
    xieTecEncodeJPEGBaseline *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecEncodeJPEGBaseline, *bufDest, dstParam);

	dstParam->interleave = srcParam->interleave;
	dstParam->bandOrder  = srcParam->band_order;
	dstParam->horizontalSamples[0] = srcParam->horizontal_samples[0];
	dstParam->horizontalSamples[1] = srcParam->horizontal_samples[1];
	dstParam->horizontalSamples[2] = srcParam->horizontal_samples[2];
	dstParam->verticalSamples[0] = srcParam->vertical_samples[0];
	dstParam->verticalSamples[1] = srcParam->vertical_samples[1];
	dstParam->verticalSamples[2] = srcParam->vertical_samples[2];
	dstParam->lenQtable  = srcParam->q_size;
	dstParam->lenACtable = srcParam->ac_size;
	dstParam->lenDCtable = srcParam->dc_size;

	END_TECHNIQUE (xieTecEncodeJPEGBaseline, *bufDest, dstParam);

	/*
	 * q_size, ac_size, dc_size should be multiple of 4,
	 * so no need to pad.
	 */

	memcpy (*bufDest, srcParam->q_table, srcParam->q_size);
	*bufDest += srcParam->q_size;
	memcpy (*bufDest, srcParam->ac_table, srcParam->ac_size);
	*bufDest += srcParam->ac_size;
	memcpy (*bufDest, srcParam->dc_table, srcParam->dc_size);
	*bufDest += srcParam->dc_size;
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieEncodeJPEGLosslessParam *)srcPtr)
int
_XieEncodeJPEGLosslessParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecEncodeJPEGLossless) +
	(srcParam->table_size >> 2);
    xieTecEncodeJPEGLossless *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecEncodeJPEGLossless, *bufDest, dstParam);

	dstParam->interleave = srcParam->interleave;
	dstParam->bandOrder = srcParam->band_order;
	dstParam->lenTable = srcParam->table_size;
	dstParam->predictor[0] = srcParam->predictor[0];
	dstParam->predictor[1] = srcParam->predictor[1];
	dstParam->predictor[2] = srcParam->predictor[2];

	END_TECHNIQUE (xieTecEncodeJPEGLossless, *bufDest, dstParam);

	/* table_size should be multiple of 4, so no need to pad */

	memcpy (*bufDest, srcParam->table, srcParam->table_size);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieEncodeTIFF2Param *)srcPtr)
int
_XieEncodeTIFF2Param (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecEncodeTIFF2);
    xieTecEncodeTIFF2 *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecEncodeTIFF2, *bufDest, dstParam);

	dstParam->encodedOrder = srcParam->encoded_order;
	dstParam->radiometric = srcParam->radiometric;

	END_TECHNIQUE (xieTecEncodeTIFF2, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieGeomAntialiasByAreaParam *)srcPtr)
int
_XieGeomAntialiasByAreaParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecGeomAntialiasByArea);
    xieTecGeomAntialiasByArea *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecGeomAntialiasByArea, *bufDest, dstParam);

	dstParam->simple = srcParam->simple;

	END_TECHNIQUE (xieTecGeomAntialiasByArea, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieGeomAntialiasByLowpassParam *)srcPtr)
int
_XieGeomAntialiasByLowpassParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecGeomAntialiasByLowpass);
    xieTecGeomAntialiasByLowpass *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecGeomAntialiasByLowpass, *bufDest, dstParam);

	dstParam->kernelSize = srcParam->kernel_size;

	END_TECHNIQUE (xieTecGeomAntialiasByLowpass, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieGeomGaussianParam *)srcPtr)
int
_XieGeomGaussianParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecGeomGaussian);
    xieTecGeomGaussian *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecGeomGaussian, *bufDest, dstParam);

	dstParam->radius = srcParam->radius;
	dstParam->simple = srcParam->simple;
	dstParam->sigma = _XieConvertToIEEE (srcParam->sigma);
	dstParam->normalize = _XieConvertToIEEE (srcParam->normalize);

	END_TECHNIQUE (xieTecGeomGaussian, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieGeomNearestNeighborParam *)srcPtr)
int
_XieGeomNearestNeighborParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    xieTecGeomNearestNeighbor 	*dstParam;
    int				length = LENOF (xieTecGeomNearestNeighbor);

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecGeomNearestNeighbor, *bufDest, dstParam);

	dstParam->modify = srcParam->modify;

	END_TECHNIQUE (xieTecGeomNearestNeighbor, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieHistogramGaussianParam	*)srcPtr)
int
_XieHistogramGaussianParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecHistogramGaussian);
    xieTecHistogramGaussian *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecHistogramGaussian, *bufDest, dstParam);

	dstParam->mean = _XieConvertToIEEE (srcParam->mean);
	dstParam->sigma = _XieConvertToIEEE (srcParam->sigma);

	END_TECHNIQUE (xieTecHistogramGaussian, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieHistogramHyperbolicParam *)srcPtr)
int
_XieHistogramHyperbolicParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecHistogramHyperbolic);
    xieTecHistogramHyperbolic *dstParam;

    if (srcParam == NULL)
	return(0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecHistogramHyperbolic, *bufDest, dstParam);

	dstParam->shapeFactor = srcParam->shape_factor;
	dstParam->constant = _XieConvertToIEEE (srcParam->constant);

	END_TECHNIQUE (xieTecHistogramHyperbolic, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam


#define srcParam ((XieWhiteAdjustCIELabShiftParam *)srcPtr)
int
_XieWhiteAdjustCIELabShiftParam (
	char **		bufDest,
	XiePointer	srcPtr,
	int		encode)
{
    int length = LENOF (xieTecWhiteAdjustCIELabShift);
    xieTecWhiteAdjustCIELabShift *dstParam;

    if (srcParam == NULL)
	return (0);

    if (encode)
    {
	BEGIN_TECHNIQUE (xieTecWhiteAdjustCIELabShift, *bufDest, dstParam);

	dstParam->whitePoint0 = _XieConvertToIEEE (srcParam->white_point[0]);
	dstParam->whitePoint1 = _XieConvertToIEEE (srcParam->white_point[1]);
	dstParam->whitePoint2 = _XieConvertToIEEE (srcParam->white_point[2]);

	END_TECHNIQUE (xieTecWhiteAdjustCIELabShift, *bufDest, dstParam);
    }

    return (length);
}
#undef srcParam



void _XieInitTechFuncTable (void)
{
    int i;

    for (i = 0; i < xieValMaxTechGroup; i++)
	_XieTechFuncs[i] = NULL;


    _XieRegisterTechFunc (xieValColorAlloc,
	xieValColorAllocDefault, 0);
    _XieRegisterTechFunc (xieValColorAlloc,
	xieValColorAllocAll, _XieColorAllocAllParam);
    _XieRegisterTechFunc (xieValColorAlloc,
	xieValColorAllocMatch, _XieColorAllocMatchParam);
    _XieRegisterTechFunc (xieValColorAlloc,
	xieValColorAllocRequantize, _XieColorAllocRequantizeParam);

    _XieRegisterTechFunc (xieValConvertFromRGB,
	xieValRGBToCIELab, _XieRGBToCIELabParam);
    _XieRegisterTechFunc (xieValConvertFromRGB,
	xieValRGBToCIEXYZ, _XieRGBToCIEXYZParam);
    _XieRegisterTechFunc (xieValConvertFromRGB,
	xieValRGBToYCbCr, _XieRGBToYCbCrParam);
    _XieRegisterTechFunc (xieValConvertFromRGB,
	xieValRGBToYCC, _XieRGBToYCCParam);

    _XieRegisterTechFunc (xieValConvertToRGB,
	xieValCIELabToRGB, _XieCIELabToRGBParam);
    _XieRegisterTechFunc (xieValConvertToRGB,
	xieValCIEXYZToRGB, _XieCIEXYZToRGBParam);
    _XieRegisterTechFunc (xieValConvertToRGB,
	xieValYCbCrToRGB, _XieYCbCrToRGBParam);
    _XieRegisterTechFunc (xieValConvertToRGB,
	xieValYCCToRGB, _XieYCCToRGBParam);

    _XieRegisterTechFunc (xieValConstrain,
	xieValConstrainClipScale, _XieClipScaleParam);
    _XieRegisterTechFunc (xieValConstrain,
	xieValConstrainHardClip, 0);

    _XieRegisterTechFunc (xieValConvolve,
	xieValConvolveDefault, 0);
    _XieRegisterTechFunc (xieValConvolve,
	xieValConvolveConstant, _XieConvolveConstantParam);
    _XieRegisterTechFunc (xieValConvolve,
	xieValConvolveReplicate, 0);

    _XieRegisterTechFunc (xieValDecode,
	xieValDecodeUncompressedSingle, _XieDecodeUncompressedSingleParam);
    _XieRegisterTechFunc (xieValDecode,
	xieValDecodeUncompressedTriple, _XieDecodeUncompressedTripleParam);
    _XieRegisterTechFunc (xieValDecode,
	xieValDecodeG31D, _XieDecodeG31DParam);
    _XieRegisterTechFunc (xieValDecode,
	xieValDecodeG32D, _XieDecodeG32DParam);
    _XieRegisterTechFunc (xieValDecode,
	xieValDecodeG42D, _XieDecodeG42DParam);
    _XieRegisterTechFunc (xieValDecode,
	xieValDecodeJPEGBaseline, _XieDecodeJPEGBaselineParam);
    _XieRegisterTechFunc (xieValDecode,
	xieValDecodeJPEGLossless, _XieDecodeJPEGLosslessParam);
    _XieRegisterTechFunc (xieValDecode,
	xieValDecodeTIFF2, _XieDecodeTIFF2Param);
    _XieRegisterTechFunc (xieValDecode,
	xieValDecodeTIFFPackBits, _XieDecodeTIFFPackBitsParam);

    _XieRegisterTechFunc (xieValDither,
	xieValDitherDefault, 0);
    _XieRegisterTechFunc (xieValDither,
	xieValDitherErrorDiffusion, 0);
    _XieRegisterTechFunc (xieValDither,
	xieValDitherOrdered, _XieDitherOrderedParam);

/*
    _XieRegisterTechFunc (xieValEncode,
	xieValEncodeServerChoice, 0);
*/
    _XieRegisterTechFunc (xieValEncode,
	xieValEncodeServerChoice, _XieEncodeServerChoiceParam);
    _XieRegisterTechFunc (xieValEncode,
	xieValEncodeUncompressedSingle, _XieEncodeUncompressedSingleParam);
    _XieRegisterTechFunc (xieValEncode,
	xieValEncodeUncompressedTriple, _XieEncodeUncompressedTripleParam);
    _XieRegisterTechFunc (xieValEncode,
	xieValEncodeG31D, _XieEncodeG31DParam);
    _XieRegisterTechFunc (xieValEncode,
	xieValEncodeG32D, _XieEncodeG32DParam);
    _XieRegisterTechFunc (xieValEncode,
	xieValEncodeG42D, _XieEncodeG42DParam);
    _XieRegisterTechFunc (xieValEncode,
	xieValEncodeJPEGBaseline, _XieEncodeJPEGBaselineParam);
    _XieRegisterTechFunc (xieValEncode,
	xieValEncodeJPEGLossless, _XieEncodeJPEGLosslessParam);
    _XieRegisterTechFunc (xieValEncode,
	xieValEncodeTIFF2, _XieEncodeTIFF2Param);
    _XieRegisterTechFunc (xieValEncode,
	xieValEncodeTIFFPackBits, _XieEncodeTIFFPackBitsParam);

    _XieRegisterTechFunc (xieValGamut,
	xieValGamutDefault, 0);
    _XieRegisterTechFunc (xieValGamut,
	xieValGamutNone, 0);
    _XieRegisterTechFunc (xieValGamut,
	xieValGamutClipRGB, 0);

    _XieRegisterTechFunc (xieValGeometry,
	xieValGeomDefault, 0);
    _XieRegisterTechFunc (xieValGeometry,
	xieValGeomAntialias, 0);
    _XieRegisterTechFunc (xieValGeometry,
	xieValGeomAntialiasByArea, _XieGeomAntialiasByAreaParam);
    _XieRegisterTechFunc (xieValGeometry,
	xieValGeomAntialiasByLPF, _XieGeomAntialiasByLowpassParam);
    _XieRegisterTechFunc (xieValGeometry,
	xieValGeomBilinearInterp, 0);
    _XieRegisterTechFunc (xieValGeometry,
	xieValGeomGaussian, _XieGeomGaussianParam);
    _XieRegisterTechFunc (xieValGeometry,
	xieValGeomNearestNeighbor, _XieGeomNearestNeighborParam);

    _XieRegisterTechFunc (xieValHistogram,
	xieValHistogramFlat, 0);
    _XieRegisterTechFunc (xieValHistogram,
	xieValHistogramGaussian, _XieHistogramGaussianParam);
    _XieRegisterTechFunc (xieValHistogram,
	xieValHistogramHyperbolic, _XieHistogramHyperbolicParam);

    _XieRegisterTechFunc (xieValWhiteAdjust,
	xieValWhiteAdjustDefault, 0);
    _XieRegisterTechFunc (xieValWhiteAdjust,
	xieValWhiteAdjustNone, 0);
    _XieRegisterTechFunc (xieValWhiteAdjust,
	xieValWhiteAdjustCIELabShift, _XieWhiteAdjustCIELabShiftParam);
}
