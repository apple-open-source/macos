/* $Xorg: pl_oc_enc.c,v 1.4 2001/02/09 02:03:28 xorgcvs Exp $ */
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

#include "PEXlib.h"
#include "PEXlibint.h"
#include "pl_oc_util.h"


char *
PEXEncodeOCs (float_format, oc_count, oc_data, length_return)

INPUT int		float_format;
INPUT unsigned long	oc_count;
INPUT PEXOCData		*oc_data;
OUTPUT unsigned long	*length_return;

{
    extern void		(*(PEX_encode_oc_funcs[]))();
    PEXOCData		*ocSrc;
    char		*ocDest, *ocRet;
    int			i;


    /*
     * Allocate a buffer to hold the encodings.
     */

    *length_return = PEXGetSizeOCs (float_format, oc_count, oc_data);
    ocRet = ocDest = (char *) Xmalloc ((unsigned) *length_return);


    /*
     * Now, encode the OCs in the buffer.
     */

    for (i = 0, ocSrc = oc_data; i < oc_count; i++, ocSrc++)
	(*PEX_encode_oc_funcs[ocSrc->oc_type]) (float_format, ocSrc, &ocDest);


#ifdef DEBUG
    if (ocDest - ocRet != *length_return)
    {
	printf ("PEXlib WARNING : Internal error in PEXEncodeOCs :\n");
	printf ("Data size encoded != Data size allocated.\n");
    }
#endif

    return (ocRet);
}


void _PEXEncodeEnumType (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexMarkerType *oc;

    BEGIN_SIMPLE_ENCODE (MarkerType, ocSrc->oc_type, *ocDest, oc);

    oc->markerType = ocSrc->data.SetMarkerType.marker_type;

    END_SIMPLE_ENCODE (MarkerType, *ocDest, oc);
}


void _PEXEncodeTableIndex (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexMarkerColorIndex *oc;

    BEGIN_SIMPLE_ENCODE (MarkerColorIndex, ocSrc->oc_type, *ocDest, oc);

    oc->index = ocSrc->data.SetMarkerColorIndex.index;

    END_SIMPLE_ENCODE (MarkerColorIndex, *ocDest, oc);
}


void _PEXEncodeColor (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexMarkerColor	*oc;
    int			lenofColor;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    lenofColor = GetColorLength (ocSrc->data.SetMarkerColor.color_type);

    BEGIN_ENCODE_OCHEADER (MarkerColor, ocSrc->oc_type,
	lenofColor, *ocDest, oc);

    oc->colorType = ocSrc->data.SetMarkerColor.color_type;

    END_ENCODE_OCHEADER (MarkerColor, *ocDest, oc);

    STORE_COLOR_VAL (ocSrc->data.SetMarkerColor.color_type,
	ocSrc->data.SetMarkerColor.color,
	*ocDest, fpConvert, fpFormat)
}


void _PEXEncodeFloat (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexMarkerScale	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    BEGIN_SIMPLE_ENCODE (MarkerScale, ocSrc->oc_type, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.SetMarkerScale.scale,
	    oc->scale, fpFormat);
    }
    else
	oc->scale = ocSrc->data.SetMarkerScale.scale;

    END_SIMPLE_ENCODE (MarkerScale, *ocDest, oc);
}


void _PEXEncodeCARD16 (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexTextPrecision	*oc;

    BEGIN_SIMPLE_ENCODE (TextPrecision, ocSrc->oc_type, *ocDest, oc);

    oc->precision = ocSrc->data.SetTextPrecision.precision;

    END_SIMPLE_ENCODE (TextPrecision, *ocDest, oc);
}


void _PEXEncodeVector2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexCharUpVector	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    BEGIN_SIMPLE_ENCODE (CharUpVector, ocSrc->oc_type, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.SetCharUpVector.vector.x,
	    oc->up_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetCharUpVector.vector.y,
	    oc->up_y, fpFormat);
    }
    else
    {
	oc->up_x = ocSrc->data.SetCharUpVector.vector.x;
	oc->up_y = ocSrc->data.SetCharUpVector.vector.y;
    }

    END_SIMPLE_ENCODE (CharUpVector, *ocDest, oc);
}


void _PEXEncodeTextAlignment (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexTextAlignment *oc;

    BEGIN_SIMPLE_ENCODE (TextAlignment, ocSrc->oc_type, *ocDest, oc);

    oc->alignment_horizontal = ocSrc->data.SetTextAlignment.halignment;
    oc->alignment_vertical = ocSrc->data.SetTextAlignment.valignment;
    
    END_SIMPLE_ENCODE (TextAlignment, *ocDest, oc);
}


void _PEXEncodeCurveApprox (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexCurveApprox	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    BEGIN_SIMPLE_ENCODE (CurveApprox, ocSrc->oc_type, *ocDest, oc);

    oc->approxMethod = ocSrc->data.SetCurveApprox.method;

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.SetCurveApprox.tolerance,
	    oc->tolerance, fpFormat);
    }	    
    else
	oc->tolerance = ocSrc->data.SetCurveApprox.tolerance;

    END_SIMPLE_ENCODE (CurveApprox, *ocDest, oc);
}


void _PEXEncodeReflectionAttr (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexReflectionAttributes	*oc;
    PEXReflectionAttributes	*reflectionAttr;
    int				lenofColor;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    reflectionAttr = &(ocSrc->data.SetReflectionAttributes.attributes);
    lenofColor = GetColorLength (reflectionAttr->specular_color.type);

    BEGIN_ENCODE_OCHEADER (ReflectionAttributes, ocSrc->oc_type,
	lenofColor, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (reflectionAttr->ambient, oc->ambient, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->diffuse, oc->diffuse, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->specular, oc->specular, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->specular_conc,
	    oc->specularConc, fpFormat);
	FP_CONVERT_HTON (reflectionAttr->transmission,
	    oc->transmission, fpFormat);
    }
    else
    {
	oc->ambient = reflectionAttr->ambient;
	oc->diffuse = reflectionAttr->diffuse;
	oc->specular = reflectionAttr->specular;
	oc->specularConc = reflectionAttr->specular_conc;
	oc->transmission = reflectionAttr->transmission;
    }

    oc->specular_colorType = reflectionAttr->specular_color.type;

    END_ENCODE_OCHEADER (ReflectionAttributes, *ocDest, oc);

    STORE_COLOR_VAL (reflectionAttr->specular_color.type,
	reflectionAttr->specular_color.value, *ocDest, fpConvert, fpFormat);
}


void _PEXEncodeSurfaceApprox (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexSurfaceApprox	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    BEGIN_SIMPLE_ENCODE (SurfaceApprox, ocSrc->oc_type, *ocDest, oc);

    oc->approxMethod = ocSrc->data.SetSurfaceApprox.method;

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.SetSurfaceApprox.utolerance,
	    oc->uTolerance, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetSurfaceApprox.vtolerance,
	    oc->vTolerance, fpFormat);
    }
    else
    {
	oc->uTolerance = ocSrc->data.SetSurfaceApprox.utolerance;
	oc->vTolerance = ocSrc->data.SetSurfaceApprox.vtolerance;
    }

    END_SIMPLE_ENCODE (SurfaceApprox, *ocDest, oc);
}


void _PEXEncodeCullMode (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexFacetCullingMode *oc;

    BEGIN_SIMPLE_ENCODE (FacetCullingMode, ocSrc->oc_type, *ocDest, oc);

    oc->cullMode = ocSrc->data.SetFacetCullingMode.mode;

    END_SIMPLE_ENCODE (FacetCullingMode, *ocDest, oc);
}


void _PEXEncodeSwitch (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexFacetDistinguishFlag *oc;

    BEGIN_SIMPLE_ENCODE (FacetDistinguishFlag, ocSrc->oc_type, *ocDest, oc);

    oc->distinguish = ocSrc->data.SetFacetDistinguishFlag.flag;

    END_SIMPLE_ENCODE (FacetDistinguishFlag, *ocDest, oc);
}


void _PEXEncodePatternSize (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexPatternSize 	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    BEGIN_SIMPLE_ENCODE (PatternSize, ocSrc->oc_type, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.SetPatternSize.width,
	    oc->size_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetPatternSize.height,
	    oc->size_y, fpFormat);
    }
    else
    {
	oc->size_x = ocSrc->data.SetPatternSize.width;
	oc->size_y = ocSrc->data.SetPatternSize.height;
    }

    END_SIMPLE_ENCODE (PatternSize, *ocDest, oc);
}


void _PEXEncodePatternAttr2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexPatternAttributes2D 	*oc;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    BEGIN_SIMPLE_ENCODE (PatternAttributes2D, ocSrc->oc_type, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.SetPatternAttributes2D.ref_point.x,
	    oc->point_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetPatternAttributes2D.ref_point.y,
	    oc->point_y, fpFormat);
    }
    else
    {
	oc->point_x = ocSrc->data.SetPatternAttributes2D.ref_point.x;
	oc->point_y = ocSrc->data.SetPatternAttributes2D.ref_point.y;
    }

    END_SIMPLE_ENCODE (PatternAttributes2D, *ocDest, oc);
}


void _PEXEncodePatternAttr (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexPatternAttributes 	*oc;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    BEGIN_SIMPLE_ENCODE (PatternAttributes, ocSrc->oc_type, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.SetPatternAttributes.ref_point.x,
	    oc->refPt_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetPatternAttributes.ref_point.y,
	    oc->refPt_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetPatternAttributes.ref_point.z,
	    oc->refPt_z, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetPatternAttributes.vector1.x,
	    oc->vector1_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetPatternAttributes.vector1.y,
	    oc->vector1_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetPatternAttributes.vector1.z,
	    oc->vector1_z, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetPatternAttributes.vector2.x,
	    oc->vector2_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetPatternAttributes.vector2.y,
	    oc->vector2_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.SetPatternAttributes.vector2.z,
	    oc->vector2_z, fpFormat);
    }
    else
    {
	oc->refPt_x = ocSrc->data.SetPatternAttributes.ref_point.x;
	oc->refPt_y = ocSrc->data.SetPatternAttributes.ref_point.y;
	oc->refPt_z = ocSrc->data.SetPatternAttributes.ref_point.z;
	oc->vector1_x = ocSrc->data.SetPatternAttributes.vector1.x;
	oc->vector1_y = ocSrc->data.SetPatternAttributes.vector1.y;
	oc->vector1_z = ocSrc->data.SetPatternAttributes.vector1.z;
	oc->vector2_x = ocSrc->data.SetPatternAttributes.vector2.x;
	oc->vector2_y = ocSrc->data.SetPatternAttributes.vector2.y;
	oc->vector2_z = ocSrc->data.SetPatternAttributes.vector2.z;
    }

    END_SIMPLE_ENCODE (PatternAttributes, *ocDest, oc);
}


void _PEXEncodeASF (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexIndividualASF *oc;
    
    BEGIN_SIMPLE_ENCODE (IndividualASF, ocSrc->oc_type, *ocDest, oc);

    oc->attribute = ocSrc->data.SetIndividualASF.attribute;
    oc->source = ocSrc->data.SetIndividualASF.asf;

    END_SIMPLE_ENCODE (IndividualASF, *ocDest, oc);
}


void _PEXEncodeLocalTransform (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexLocalTransform	*oc;
    char		*ptr;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    BEGIN_SIMPLE_ENCODE (LocalTransform, ocSrc->oc_type, *ocDest, oc);

    oc->compType = ocSrc->data.SetLocalTransform.composition;

    ptr = (char *) oc->matrix;
    STORE_LISTOF_FLOAT32 (16, ocSrc->data.SetLocalTransform.transform, ptr,
	fpConvert, fpFormat);

    END_SIMPLE_ENCODE (LocalTransform, *ocDest, oc);
}


void _PEXEncodeLocalTransform2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexLocalTransform2D	*oc;
    char		*ptr;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    BEGIN_SIMPLE_ENCODE (LocalTransform2D, ocSrc->oc_type, *ocDest, oc);

    oc->compType = ocSrc->data.SetLocalTransform2D.composition;

    ptr = (char *) oc->matrix3X3;
    STORE_LISTOF_FLOAT32 (9, ocSrc->data.SetLocalTransform2D.transform, ptr,
	fpConvert, fpFormat);

    END_SIMPLE_ENCODE (LocalTransform2D, *ocDest, oc);
}


void _PEXEncodeGlobalTransform (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexGlobalTransform	*oc;
    char		*ptr;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    BEGIN_SIMPLE_ENCODE (GlobalTransform, ocSrc->oc_type, *ocDest, oc);

    ptr = (char *) oc->matrix;
    STORE_LISTOF_FLOAT32 (16, ocSrc->data.SetGlobalTransform.transform, ptr,
	fpConvert, fpFormat);

    END_SIMPLE_ENCODE (GlobalTransform, *ocDest, oc);
}


void _PEXEncodeGlobalTransform2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexGlobalTransform2D	*oc;
    char			*ptr;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    BEGIN_SIMPLE_ENCODE (GlobalTransform2D, ocSrc->oc_type, *ocDest, oc);

    ptr = (char *) oc->matrix3X3;
    STORE_LISTOF_FLOAT32 (9, ocSrc->data.SetGlobalTransform2D.transform, ptr,
	fpConvert, fpFormat);

    END_SIMPLE_ENCODE (GlobalTransform2D, *ocDest, oc);
}


void _PEXEncodeModelClipVolume (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexModelClipVolume	*oc;
    int			dataLength;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    dataLength = NUMWORDS (SIZEOF (pexHalfSpace) *
	ocSrc->data.SetModelClipVolume.count);
    
    BEGIN_ENCODE_OCHEADER (ModelClipVolume, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->modelClipOperator = ocSrc->data.SetModelClipVolume.op;
    oc->numHalfSpaces = ocSrc->data.SetModelClipVolume.count;

    END_ENCODE_OCHEADER (ModelClipVolume, *ocDest, oc);

    STORE_LISTOF_HALFSPACE3D (ocSrc->data.SetModelClipVolume.count,
	ocSrc->data.SetModelClipVolume.half_spaces, *ocDest,
 	fpConvert, fpFormat);
}


void _PEXEncodeModelClipVolume2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexModelClipVolume2D	*oc;
    int				dataLength;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    dataLength = NUMWORDS (SIZEOF (pexHalfSpace2D) *
	ocSrc->data.SetModelClipVolume2D.count);
    
    BEGIN_ENCODE_OCHEADER (ModelClipVolume2D, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->modelClipOperator = ocSrc->data.SetModelClipVolume2D.op;
    oc->numHalfSpaces = ocSrc->data.SetModelClipVolume2D.count;

    END_ENCODE_OCHEADER (ModelClipVolume2D, *ocDest, oc);

    STORE_LISTOF_HALFSPACE2D (ocSrc->data.SetModelClipVolume2D.count,
	ocSrc->data.SetModelClipVolume2D.half_spaces, *ocDest,
 	fpConvert, fpFormat);
}


void _PEXEncodeRestoreModelClip (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexRestoreModelClipVolume *oc;

    BEGIN_SIMPLE_ENCODE (RestoreModelClipVolume, ocSrc->oc_type, *ocDest, oc);
    /* no data */
    END_SIMPLE_ENCODE (RestoreModelClipVolume, *ocDest, oc);
}


void _PEXEncodeLightSourceState (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexLightSourceState	*oc;
    int			dataLength;

    dataLength =
	NUMWORDS (SIZEOF (CARD16) * 
	ocSrc->data.SetLightSourceState.enable_count) + 
	NUMWORDS (SIZEOF (CARD16) * 
	ocSrc->data.SetLightSourceState.disable_count);

    BEGIN_ENCODE_OCHEADER (LightSourceState, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->numEnable = ocSrc->data.SetLightSourceState.enable_count;
    oc->numDisable = ocSrc->data.SetLightSourceState.disable_count;

    END_ENCODE_OCHEADER (LightSourceState, *ocDest, oc);

    STORE_LISTOF_CARD16 (ocSrc->data.SetLightSourceState.enable_count,
	ocSrc->data.SetLightSourceState.enable, *ocDest);

    if (ocSrc->data.SetLightSourceState.enable_count & 1)
	*ocDest += 2;

    STORE_LISTOF_CARD16 (ocSrc->data.SetLightSourceState.disable_count,
	ocSrc->data.SetLightSourceState.disable, *ocDest);

    if (ocSrc->data.SetLightSourceState.disable_count & 1)
	*ocDest += 2;
}


void _PEXEncodeID (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexPickID *oc;

    BEGIN_SIMPLE_ENCODE (PickID, ocSrc->oc_type, *ocDest, oc);

    oc->pickId = ocSrc->data.SetPickID.pick_id;

    END_SIMPLE_ENCODE (PickID, *ocDest, oc);
}


void _PEXEncodePSC (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexParaSurfCharacteristics 	*oc;
    PEXPSCData			*pscData;
    int				pscType;
    int				dataLength = 0;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    pscType = ocSrc->data.SetParaSurfCharacteristics.psc_type;
    pscData = &(ocSrc->data.SetParaSurfCharacteristics.characteristics);
    
    if (pscType == PEXPSCIsoCurves)
    {
	dataLength = LENOF (pexPSC_IsoparametricCurves);
    }
    else if (pscType == PEXPSCMCLevelCurves || pscType == PEXPSCWCLevelCurves)
    {
	dataLength = NUMWORDS (SIZEOF (pexPSC_LevelCurves) +
		(pscData->level_curves.count * SIZEOF (float)));
    }

    BEGIN_ENCODE_OCHEADER (ParaSurfCharacteristics, ocSrc->oc_type,
	dataLength, *ocDest, oc);
    
    oc->characteristics = pscType;
    oc->length = NUMBYTES (dataLength);

    END_ENCODE_OCHEADER (ParaSurfCharacteristics, *ocDest, oc);

    if (dataLength > 0)
    {
	if (pscType == PEXPSCIsoCurves)
	{
	    STORE_PSC_ISOCURVES (pscData->iso_curves, *ocDest);
	}
	else if (pscType == PEXPSCMCLevelCurves ||
	    pscType == PEXPSCWCLevelCurves)
	{
	    STORE_PSC_LEVELCURVES (pscData->level_curves, *ocDest,
		fpConvert, fpFormat);

	    STORE_LISTOF_FLOAT32 (pscData->level_curves.count,
		pscData->level_curves.parameters, *ocDest,
		fpConvert, fpFormat);
	}
    }
}


void _PEXEncodeNameSet (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexAddToNameSet	*oc;
    int			dataLength;

    dataLength = NUMWORDS (ocSrc->data.AddToNameSet.count * SIZEOF (pexName));

    BEGIN_ENCODE_OCHEADER (AddToNameSet, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    END_ENCODE_OCHEADER (AddToNameSet, *ocDest, oc);

    STORE_LISTOF_CARD32 (ocSrc->data.AddToNameSet.count,
	ocSrc->data.AddToNameSet.names, *ocDest);
}


void _PEXEncodeExecuteStructure (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexExecuteStructure *oc;

    BEGIN_SIMPLE_ENCODE (ExecuteStructure, ocSrc->oc_type, *ocDest, oc);

    oc->id = ocSrc->data.ExecuteStructure.structure;

    END_SIMPLE_ENCODE (ExecuteStructure, *ocDest, oc);
}


void _PEXEncodeLabel (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexLabel *oc;

    BEGIN_SIMPLE_ENCODE (Label, ocSrc->oc_type, *ocDest, oc);

    oc->label = ocSrc->data.Label.label;

    END_SIMPLE_ENCODE (Label, *ocDest, oc);
}


void _PEXEncodeApplicationData (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexApplicationData  *oc;
    int			dataLength;

    dataLength = NUMWORDS (ocSrc->data.ApplicationData.length);

    BEGIN_ENCODE_OCHEADER (ApplicationData, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->numElements = ocSrc->data.ApplicationData.length;

    END_ENCODE_OCHEADER (ApplicationData, *ocDest, oc);

    memcpy (*ocDest, ocSrc->data.ApplicationData.data,
	ocSrc->data.ApplicationData.length);

    *ocDest += PADDED_BYTES (ocSrc->data.ApplicationData.length);
}


void _PEXEncodeGSE (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexGSE 	*oc;
    int		dataLength;
    
    dataLength = NUMWORDS (ocSrc->data.GSE.length);

    BEGIN_ENCODE_OCHEADER (GSE, ocSrc->oc_type, dataLength, *ocDest, oc);

    oc->id = ocSrc->data.GSE.id;
    oc->numElements = ocSrc->data.GSE.length;

    END_ENCODE_OCHEADER (GSE, *ocDest, oc);

    memcpy (*ocDest, ocSrc->data.GSE.data, ocSrc->data.GSE.length);
    *ocDest += PADDED_BYTES (ocSrc->data.GSE.length);
}


void _PEXEncodeMarkers (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexMarkers 		*oc;
    int			dataLength;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    dataLength = NUMWORDS (ocSrc->data.Markers.count * SIZEOF (pexCoord3D));

    BEGIN_ENCODE_OCHEADER (Markers, ocSrc->oc_type, dataLength, *ocDest, oc);
    END_ENCODE_OCHEADER (Markers, *ocDest, oc);

    STORE_LISTOF_COORD3D (ocSrc->data.Markers.count,
	ocSrc->data.Markers.points, *ocDest, fpConvert, fpFormat);
}


void _PEXEncodePolyline (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexPolyline		*oc;
    int			dataLength;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    dataLength = NUMWORDS (ocSrc->data.Polyline.count * SIZEOF (pexCoord3D));

    BEGIN_ENCODE_OCHEADER (Polyline, ocSrc->oc_type, dataLength, *ocDest, oc);
    END_ENCODE_OCHEADER (Polyline, *ocDest, oc);

    STORE_LISTOF_COORD3D (ocSrc->data.Polyline.count,
	ocSrc->data.Polyline.points, *ocDest, fpConvert, fpFormat);
}


void _PEXEncodeMarkers2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexMarkers2D	*oc;
    int			dataLength;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    dataLength = NUMWORDS (ocSrc->data.Markers2D.count * SIZEOF (pexCoord2D));

    BEGIN_ENCODE_OCHEADER (Markers2D, ocSrc->oc_type,
	dataLength, *ocDest, oc);
    END_ENCODE_OCHEADER (Markers2D, *ocDest, oc);

    STORE_LISTOF_COORD2D (ocSrc->data.Markers2D.count,
	ocSrc->data.Markers2D.points, *ocDest, fpConvert, fpFormat);
}


void _PEXEncodePolyline2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexPolyline2D	*oc;
    int			dataLength;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    dataLength = NUMWORDS (ocSrc->data.Polyline2D.count * SIZEOF (pexCoord2D));

    BEGIN_ENCODE_OCHEADER (Polyline2D, ocSrc->oc_type,
	dataLength, *ocDest, oc);
    END_ENCODE_OCHEADER (Polyline2D, *ocDest, oc);

    STORE_LISTOF_COORD2D (ocSrc->data.Polyline2D.count,
	ocSrc->data.Polyline2D.points, *ocDest, fpConvert, fpFormat);
}


void _PEXEncodeText (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    /* Text is always mono encoded */
    
    pexText		*oc;
    int 		lenofStrings;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    GetStringsLength (ocSrc->data.EncodedText.count,
	ocSrc->data.EncodedText.encoded_text, lenofStrings);
    
    BEGIN_ENCODE_OCHEADER (Text, ocSrc->oc_type, lenofStrings, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.EncodedText.origin.x,
	    oc->origin_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedText.origin.y,
	    oc->origin_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedText.origin.z,
	    oc->origin_z, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedText.vector1.x,
	    oc->vector1_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedText.vector1.y,
	    oc->vector1_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedText.vector1.z,
	    oc->vector1_z, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedText.vector2.x,
	    oc->vector2_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedText.vector2.y,
	    oc->vector2_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedText.vector2.z,
	    oc->vector2_z, fpFormat);
    }
    else
    {
	oc->origin_x = ocSrc->data.EncodedText.origin.x;
	oc->origin_y = ocSrc->data.EncodedText.origin.y;
	oc->origin_z = ocSrc->data.EncodedText.origin.z;
	oc->vector1_x = ocSrc->data.EncodedText.vector1.x;
	oc->vector1_y = ocSrc->data.EncodedText.vector1.y;
	oc->vector1_z = ocSrc->data.EncodedText.vector1.z;
	oc->vector2_x = ocSrc->data.EncodedText.vector2.x;
	oc->vector2_y = ocSrc->data.EncodedText.vector2.y;
	oc->vector2_z = ocSrc->data.EncodedText.vector2.z;
    }

    oc->numEncodings = ocSrc->data.EncodedText.count;
    
    END_ENCODE_OCHEADER (Text, *ocDest, oc);

    STORE_LISTOF_MONOENCODING (ocSrc->data.EncodedText.count,
	ocSrc->data.EncodedText.encoded_text, *ocDest);
}


void _PEXEncodeText2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    /* Text is always mono encoded */
    
    pexText2D		*oc;
    int 		lenofStrings;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    GetStringsLength (ocSrc->data.EncodedText2D.count,
	ocSrc->data.EncodedText2D.encoded_text, lenofStrings);
    
    BEGIN_ENCODE_OCHEADER (Text2D, ocSrc->oc_type, lenofStrings, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.EncodedText2D.origin.x,
	    oc->origin_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedText2D.origin.y,
	    oc->origin_y, fpFormat);
    }
    else
    {
	oc->origin_x = ocSrc->data.EncodedText2D.origin.x;
	oc->origin_y = ocSrc->data.EncodedText2D.origin.y;
    }

    oc->numEncodings = ocSrc->data.EncodedText2D.count;
    
    END_ENCODE_OCHEADER (Text2D, *ocDest, oc);

    STORE_LISTOF_MONOENCODING (ocSrc->data.EncodedText2D.count,
	ocSrc->data.EncodedText2D.encoded_text, *ocDest);
}


void _PEXEncodeAnnoText (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    /* Anno Text is always mono encoded */
    
    pexAnnotationText	*oc;
    int 		lenofStrings;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    GetStringsLength (ocSrc->data.EncodedAnnoText.count,
	ocSrc->data.EncodedAnnoText.encoded_text, lenofStrings);
    
    BEGIN_ENCODE_OCHEADER (AnnotationText, ocSrc->oc_type,
	lenofStrings, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.EncodedAnnoText.origin.x,
	    oc->origin_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedAnnoText.origin.y,
	    oc->origin_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedAnnoText.origin.z,
	    oc->origin_z, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedAnnoText.offset.x,
	    oc->offset_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedAnnoText.offset.y,
	    oc->offset_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedAnnoText.offset.z,
	    oc->offset_z, fpFormat);
    }
    else
    {
	oc->origin_x = ocSrc->data.EncodedAnnoText.origin.x;
	oc->origin_y = ocSrc->data.EncodedAnnoText.origin.y;
	oc->origin_z = ocSrc->data.EncodedAnnoText.origin.z;
	oc->offset_x = ocSrc->data.EncodedAnnoText.offset.x;
	oc->offset_y = ocSrc->data.EncodedAnnoText.offset.y;
	oc->offset_z = ocSrc->data.EncodedAnnoText.offset.z;
    }

    oc->numEncodings = ocSrc->data.EncodedAnnoText.count;
    
    END_ENCODE_OCHEADER (AnnotationText, *ocDest, oc);

    STORE_LISTOF_MONOENCODING (ocSrc->data.EncodedAnnoText.count,
	ocSrc->data.EncodedAnnoText.encoded_text, *ocDest);
}


void _PEXEncodeAnnoText2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    /* Anno Text is always mono encoded */
    
    pexAnnotationText2D	*oc;
    int 		lenofStrings;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    GetStringsLength (ocSrc->data.EncodedAnnoText2D.count,
	ocSrc->data.EncodedAnnoText2D.encoded_text, lenofStrings);
    
    BEGIN_ENCODE_OCHEADER (AnnotationText2D, ocSrc->oc_type,
	lenofStrings, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.EncodedAnnoText2D.origin.x,
	    oc->origin_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedAnnoText2D.origin.y,
	    oc->origin_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedAnnoText2D.offset.x,
	    oc->offset_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.EncodedAnnoText2D.offset.y,
	    oc->offset_y, fpFormat);
    }
    else
    {
	oc->origin_x = ocSrc->data.EncodedAnnoText2D.origin.x;
	oc->origin_y = ocSrc->data.EncodedAnnoText2D.origin.y;
	oc->offset_x = ocSrc->data.EncodedAnnoText2D.offset.x;
	oc->offset_y = ocSrc->data.EncodedAnnoText2D.offset.y;
    }

    oc->numEncodings = ocSrc->data.EncodedAnnoText2D.count;
    
    END_ENCODE_OCHEADER (AnnotationText2D, *ocDest, oc);

    STORE_LISTOF_MONOENCODING (ocSrc->data.EncodedAnnoText2D.count,
	ocSrc->data.EncodedAnnoText2D.encoded_text, *ocDest);
}


void _PEXEncodePolylineSet (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexPolylineSetWithData	*oc;
    int				numPoints, i;
    int				dataLength;
    int				lenofVertex;
    unsigned int		vertexAttributes;
    unsigned int		numPolylines;
    int				colorType;
    PEXListOfVertex		*polylines;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    numPolylines = ocSrc->data.PolylineSetWithData.count;
    polylines = ocSrc->data.PolylineSetWithData.vertex_lists;
    colorType = ocSrc->data.PolylineSetWithData.color_type;
    vertexAttributes = ocSrc->data.PolylineSetWithData.vertex_attributes;
    
    for (i = 0, numPoints = 0; i < numPolylines; i++)
	numPoints += polylines[i].count;
    
    lenofVertex = LENOF (pexCoord3D) + ((vertexAttributes & PEXGAColor) ?
	GetColorLength (colorType) : 0); 
    
    dataLength = numPolylines + (numPoints * lenofVertex);

    BEGIN_ENCODE_OCHEADER (PolylineSetWithData, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->colorType = colorType;
    oc->vertexAttribs = vertexAttributes;
    oc->numLists = numPolylines;
    
    END_ENCODE_OCHEADER (PolylineSetWithData, *ocDest, oc);

    for (i = 0; i < numPolylines; i++)
    {
	STORE_CARD32 (polylines[i].count, *ocDest);
	
	STORE_LISTOF_VERTEX (polylines[i].count, NUMBYTES (lenofVertex),
	    colorType, vertexAttributes, polylines[i].vertices,
	    *ocDest, fpConvert, fpFormat);
    }
}


void _PEXEncodeNURBCurve (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexNURBCurve 	*oc;
    int			lenofVertexList;
    int			lenofKnotList;
    unsigned int	numPoints;
    int			rationality, order;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    numPoints = ocSrc->data.NURBCurve.count;
    rationality = ocSrc->data.NURBCurve.rationality;
    order = ocSrc->data.NURBCurve.order;
    
    lenofVertexList = numPoints * ((rationality == PEXRational) ?
	LENOF (pexCoord4D) : LENOF (pexCoord3D));
    lenofKnotList = order + numPoints;
    
    BEGIN_ENCODE_OCHEADER (NURBCurve, ocSrc->oc_type,
	lenofKnotList + lenofVertexList, *ocDest, oc);

    oc->curveOrder = order;
    oc->coordType = rationality;
    oc->numKnots = order + numPoints;
    oc->numPoints = numPoints;
    
    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.NURBCurve.tmin, oc->tmin, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.NURBCurve.tmax, oc->tmax, fpFormat);
    }
    else
    {
	oc->tmin = ocSrc->data.NURBCurve.tmin;
	oc->tmax = ocSrc->data.NURBCurve.tmax;
    }

    END_ENCODE_OCHEADER (NURBCurve, *ocDest, oc);

    STORE_LISTOF_FLOAT32 (lenofKnotList, ocSrc->data.NURBCurve.knots, *ocDest,
	fpConvert, fpFormat);

    if (rationality == PEXRational)
    {
	STORE_LISTOF_COORD4D (numPoints, ocSrc->data.NURBCurve.points.point_4d,
            *ocDest, fpConvert, fpFormat);
    }
    else
    {
	STORE_LISTOF_COORD3D (numPoints, ocSrc->data.NURBCurve.points.point,
            *ocDest, fpConvert, fpFormat);
    }
}


void _PEXEncodeFillArea (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexFillArea		*oc;
    int			dataLength;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    dataLength = ocSrc->data.FillArea.count * LENOF (pexCoord3D);
    
    BEGIN_ENCODE_OCHEADER (FillArea, ocSrc->oc_type, dataLength, *ocDest, oc);

    oc->shape = ocSrc->data.FillArea.shape_hint;
    oc->ignoreEdges = ocSrc->data.FillArea.ignore_edges;
    
    END_ENCODE_OCHEADER (FillArea, *ocDest, oc);

    STORE_LISTOF_COORD3D (ocSrc->data.FillArea.count,
	ocSrc->data.FillArea.points, *ocDest, fpConvert, fpFormat);
}


void _PEXEncodeFillArea2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexFillArea2D	*oc;
    int			dataLength;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    dataLength = ocSrc->data.FillArea2D.count * LENOF (pexCoord2D);
    
    BEGIN_ENCODE_OCHEADER (FillArea2D, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->shape = ocSrc->data.FillArea2D.shape_hint;
    oc->ignoreEdges = ocSrc->data.FillArea2D.ignore_edges;
    
    END_ENCODE_OCHEADER (FillArea2D, *ocDest, oc);

    STORE_LISTOF_COORD2D (ocSrc->data.FillArea2D.count,
	ocSrc->data.FillArea2D.points, *ocDest, fpConvert, fpFormat);
}


void _PEXEncodeFillAreaWithData (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexFillAreaWithData	*oc;
    int			dataLength;
    int			lenofFacet;
    int			lenofVertex;
    int			lenofColor;
    unsigned int	facetAttributes;
    unsigned int	vertexAttributes;
    int			colorType;
    unsigned int	numVertices;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    colorType = ocSrc->data.FillAreaWithData.color_type;
    facetAttributes = ocSrc->data.FillAreaWithData.facet_attributes;
    vertexAttributes = ocSrc->data.FillAreaWithData.vertex_attributes;
    numVertices = ocSrc->data.FillAreaWithData.count;
    
    lenofColor = GetColorLength (colorType);
    lenofFacet = GetFacetDataLength (facetAttributes, lenofColor); 
    lenofVertex = GetVertexWithDataLength (vertexAttributes, lenofColor);
    
    dataLength = lenofFacet + 1 /* count */ + numVertices * lenofVertex;

    BEGIN_ENCODE_OCHEADER (FillAreaWithData, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->shape = ocSrc->data.FillAreaWithData.shape_hint;
    oc->ignoreEdges = ocSrc->data.FillAreaWithData.ignore_edges;
    oc->colorType = colorType;
    oc->facetAttribs = facetAttributes;
    oc->vertexAttribs = vertexAttributes;
    
    END_ENCODE_OCHEADER (FillAreaWithData, *ocDest, oc);

    if (facetAttributes)
    {
	STORE_FACET (colorType, facetAttributes,
	    ocSrc->data.FillAreaWithData.facet_data, *ocDest,
	    fpConvert, fpFormat);
    }
    
    STORE_CARD32 (numVertices, *ocDest);
    
    STORE_LISTOF_VERTEX (numVertices, NUMBYTES (lenofVertex), colorType,
	vertexAttributes, ocSrc->data.FillAreaWithData.vertices, *ocDest,
	fpConvert, fpFormat);
}


void _PEXEncodeFillAreaSet (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexFillAreaSet	*oc;
    int			dataLength;
    int			numPoints, i;
    unsigned int	numFillAreas;
    PEXListOfCoord	*vertices;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    numFillAreas = ocSrc->data.FillAreaSet.count;
    vertices = ocSrc->data.FillAreaSet.point_lists;
    
    for (i = 0, numPoints = 0; i < numFillAreas; i++)
	numPoints += vertices[i].count;
    
    dataLength = numFillAreas + (numPoints * LENOF (pexCoord3D));

    BEGIN_ENCODE_OCHEADER (FillAreaSet, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->shape = ocSrc->data.FillAreaSet.shape_hint; 
    oc->ignoreEdges = ocSrc->data.FillAreaSet.ignore_edges;
    oc->contourHint = ocSrc->data.FillAreaSet.contour_hint;
    oc->numLists = numFillAreas;
    
    END_ENCODE_OCHEADER (FillAreaSet, *ocDest, oc);

    for (i = 0; i < numFillAreas; i++)
    {
	STORE_CARD32 (vertices[i].count, *ocDest);
	
	STORE_LISTOF_COORD3D (vertices[i].count, vertices[i].points, *ocDest,
	    fpConvert, fpFormat);
    }
}


void _PEXEncodeFillAreaSet2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexFillAreaSet2D	*oc;
    int			dataLength;
    int			numPoints, i;
    unsigned int	numFillAreas;
    PEXListOfCoord2D	*vertices;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    numFillAreas = ocSrc->data.FillAreaSet2D.count;
    vertices = ocSrc->data.FillAreaSet2D.point_lists;
    
    for (i = 0, numPoints = 0; i < numFillAreas; i++)
	numPoints += vertices[i].count;
    
    dataLength = numFillAreas /* counts */ + (numPoints * LENOF (pexCoord2D));

    BEGIN_ENCODE_OCHEADER (FillAreaSet2D, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->shape = ocSrc->data.FillAreaSet2D.shape_hint; 
    oc->ignoreEdges = ocSrc->data.FillAreaSet2D.ignore_edges;
    oc->contourHint = ocSrc->data.FillAreaSet2D.contour_hint;
    oc->numLists = numFillAreas;
    
    END_ENCODE_OCHEADER (FillAreaSet2D, *ocDest, oc);

    for (i = 0; i < numFillAreas; i++)
    {
	STORE_CARD32 (vertices[i].count, *ocDest);
	
	STORE_LISTOF_COORD2D (vertices[i].count, vertices[i].points, *ocDest,
	    fpConvert, fpFormat);
    }
}


void _PEXEncodeFillAreaSetWithData (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexFillAreaSetWithData	*oc;
    int				dataLength;
    int				lenofColor;
    int				lenofFacet;
    int				lenofVertex;
    int				numVertices, i;
    int				colorType;
    unsigned int		numFillAreas;
    unsigned int		facetAttributes;
    unsigned int		vertexAttributes;
    PEXListOfVertex		*vertices;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    colorType = ocSrc->data.FillAreaSetWithData.color_type;
    numFillAreas = ocSrc->data.FillAreaSetWithData.count;
    facetAttributes = ocSrc->data.FillAreaSetWithData.facet_attributes;
    vertexAttributes = ocSrc->data.FillAreaSetWithData.vertex_attributes;
    vertices = ocSrc->data.FillAreaSetWithData.vertex_lists;
    
    lenofColor = GetColorLength (colorType);
    lenofFacet = GetFacetDataLength (facetAttributes, lenofColor); 
    lenofVertex = GetVertexWithDataLength (vertexAttributes, lenofColor);
    
    if (vertexAttributes & PEXGAEdges)
	lenofVertex++; 			/* edge switch is CARD32 */
    
    for (i = 0, numVertices = 0; i < numFillAreas; i++)
	numVertices += vertices[i].count;
    
    dataLength = lenofFacet + numFillAreas + numVertices * lenofVertex;

    BEGIN_ENCODE_OCHEADER (FillAreaSetWithData, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->shape = ocSrc->data.FillAreaSetWithData.shape_hint;
    oc->ignoreEdges = ocSrc->data.FillAreaSetWithData.ignore_edges;
    oc->contourHint = ocSrc->data.FillAreaSetWithData.contour_hint;
    oc->colorType = colorType;
    oc->facetAttribs = facetAttributes;
    oc->vertexAttribs = vertexAttributes;
    oc->numLists = numFillAreas;
    
    END_ENCODE_OCHEADER (FillAreaSetWithData, *ocDest, oc);

    if (facetAttributes)
    {
	STORE_FACET (colorType, facetAttributes,
	    ocSrc->data.FillAreaSetWithData.facet_data, *ocDest,
	    fpConvert, fpFormat);
    }
    
    for (i = 0; i < numFillAreas; i++)
    {
	STORE_CARD32 (vertices[i].count, *ocDest);
	
	STORE_LISTOF_VERTEX (vertices[i].count, NUMBYTES (lenofVertex),
	    colorType, vertexAttributes, vertices[i].vertices, *ocDest,
	    fpConvert, fpFormat);
    }
}


void _PEXEncodeTriangleStrip (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexTriangleStrip	*oc;
    int			dataLength;
    int			lenofColor;
    int			lenofFacet;
    int			lenofVertex;
    int			colorType;
    unsigned long	numVertices;
    unsigned int	facetAttributes;
    unsigned int	vertexAttributes;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    colorType = ocSrc->data.TriangleStrip.color_type;
    numVertices = ocSrc->data.TriangleStrip.count;
    facetAttributes = ocSrc->data.TriangleStrip.facet_attributes;
    vertexAttributes = ocSrc->data.TriangleStrip.vertex_attributes;
    
    lenofColor = GetColorLength (colorType);
    lenofFacet = GetFacetDataLength (facetAttributes, lenofColor); 
    lenofVertex = GetVertexWithDataLength (vertexAttributes, lenofColor);

    dataLength = (numVertices - 2) * lenofFacet + numVertices * lenofVertex;

    BEGIN_ENCODE_OCHEADER (TriangleStrip, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->colorType = colorType;
    oc->facetAttribs = facetAttributes;
    oc->vertexAttribs = vertexAttributes;
    oc->numVertices = numVertices;
    
    END_ENCODE_OCHEADER (TriangleStrip, *ocDest, oc);

    if (facetAttributes)
    {
	STORE_LISTOF_FACET ((numVertices - 2),
	    NUMBYTES (lenofFacet), colorType,
	    facetAttributes, ocSrc->data.TriangleStrip.facet_data,
	    *ocDest, fpConvert, fpFormat);
    }
    
    STORE_LISTOF_VERTEX (numVertices, NUMBYTES (lenofVertex), colorType,
	vertexAttributes, ocSrc->data.TriangleStrip.vertices, *ocDest,
	fpConvert, fpFormat);
}


void _PEXEncodeQuadMesh (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexQuadrilateralMesh 	*oc;
    int				dataLength;
    int				lenofColor;
    int				lenofFacet;
    int				lenofVertex;
    int				colorType;
    unsigned int		rowCount;
    unsigned int		colCount;
    unsigned int		facetAttributes;
    unsigned int		vertexAttributes;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    colorType = ocSrc->data.QuadrilateralMesh.color_type;
    rowCount = ocSrc->data.QuadrilateralMesh.row_count;
    colCount = ocSrc->data.QuadrilateralMesh.col_count;
    facetAttributes = ocSrc->data.QuadrilateralMesh.facet_attributes;
    vertexAttributes = ocSrc->data.QuadrilateralMesh.vertex_attributes;
    
    lenofColor = GetColorLength (colorType);
    lenofFacet = GetFacetDataLength (facetAttributes, lenofColor); 
    lenofVertex = GetVertexWithDataLength (vertexAttributes, lenofColor);

    dataLength = (((rowCount - 1) * (colCount - 1)) * lenofFacet) +
	(rowCount * colCount * lenofVertex);

    BEGIN_ENCODE_OCHEADER (QuadrilateralMesh, ocSrc->oc_type,
	dataLength, *ocDest, oc);
    
    oc->colorType = colorType;
    oc->mPts = colCount;
    oc->nPts = rowCount;
    oc->facetAttribs = facetAttributes;
    oc->vertexAttribs = vertexAttributes;
    oc->shape = ocSrc->data.QuadrilateralMesh.shape_hint;
    
    END_ENCODE_OCHEADER (QuadrilateralMesh, *ocDest, oc);

    if (facetAttributes)
    {
	STORE_LISTOF_FACET ((rowCount - 1) * (colCount - 1),
	    NUMBYTES (lenofFacet), colorType, facetAttributes,
	    ocSrc->data.QuadrilateralMesh.facet_data,
	    *ocDest, fpConvert, fpFormat);
    }
    
    STORE_LISTOF_VERTEX (rowCount * colCount, NUMBYTES (lenofVertex),
	colorType, vertexAttributes, ocSrc->data.QuadrilateralMesh.vertices,
	*ocDest, fpConvert, fpFormat);
}


void _PEXEncodeSOFA (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexSetOfFillAreaSets	*oc;
    PEXConnectivityData 	*pConnectivity;
    PEXListOfUShort 		*pList;
    int 			lenofColor;
    int 			lenofFacet;
    int 			lenofVertex;
    int 			sizeofEdge;
    int				totLength;
    int 			numContours;
    int 			count;
    int				i, j;
    unsigned int		facetAttributes;
    unsigned int		vertexAttributes;
    unsigned int		edgeAttributes;
    int				colorType, cbytes;
    unsigned int		numFillAreaSets;
    unsigned int		numVertices;
    unsigned int		numIndices;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    colorType = ocSrc->data.SetOfFillAreaSets.color_type;
    facetAttributes = ocSrc->data.SetOfFillAreaSets.facet_attributes;
    vertexAttributes = ocSrc->data.SetOfFillAreaSets.vertex_attributes;
    edgeAttributes = ocSrc->data.SetOfFillAreaSets.edge_attributes;
    numFillAreaSets = ocSrc->data.SetOfFillAreaSets.set_count;
    numVertices = ocSrc->data.SetOfFillAreaSets.vertex_count;
    numIndices = ocSrc->data.SetOfFillAreaSets.index_count;
    
    numContours = 0;
    pConnectivity = ocSrc->data.SetOfFillAreaSets.connectivity;
    for (i = 0; i < numFillAreaSets; i++, pConnectivity++)
	numContours += pConnectivity->count;
    
    lenofColor = GetColorLength (colorType);
    lenofFacet = GetFacetDataLength (facetAttributes, lenofColor); 
    lenofVertex = GetVertexWithDataLength (vertexAttributes, lenofColor);
    sizeofEdge = edgeAttributes ? SIZEOF (CARD8) : 0;
    
    cbytes = SIZEOF (CARD16) * (numFillAreaSets + numContours + numIndices);

    totLength = (lenofFacet * numFillAreaSets) + (lenofVertex * numVertices) + 
	NUMWORDS (sizeofEdge * numIndices) + NUMWORDS (cbytes);
    
    BEGIN_ENCODE_OCHEADER (SetOfFillAreaSets, ocSrc->oc_type,
	totLength, *ocDest, oc);

    oc->shape = ocSrc->data.SetOfFillAreaSets.shape_hint;
    oc->colorType = colorType;
    oc->FAS_Attributes = facetAttributes;
    oc->vertexAttributes = vertexAttributes;
    oc->edgeAttributes = edgeAttributes ? PEXOn : PEXOff;
    oc->contourHint = ocSrc->data.SetOfFillAreaSets.contour_hint;
    oc->contourCountsFlag = ocSrc->data.SetOfFillAreaSets.contours_all_one;
    oc->numFAS = numFillAreaSets;
    oc->numVertices = numVertices;
    oc->numEdges = numIndices;
    oc->numContours = numContours;
    
    END_ENCODE_OCHEADER (SetOfFillAreaSets, *ocDest, oc);

    if (facetAttributes)
    {
	STORE_LISTOF_FACET (numFillAreaSets, NUMBYTES (lenofFacet), colorType,
	    facetAttributes, ocSrc->data.SetOfFillAreaSets.facet_data,
	    *ocDest, fpConvert, fpFormat);
    }
    
    STORE_LISTOF_VERTEX (numVertices, NUMBYTES (lenofVertex), colorType,
	vertexAttributes, ocSrc->data.SetOfFillAreaSets.vertices,
	*ocDest, fpConvert, fpFormat);

    if (edgeAttributes)
    {
	memcpy (*ocDest, ocSrc->data.SetOfFillAreaSets.edge_flags, numIndices);

	*ocDest += PADDED_BYTES (numIndices);
    }
    
    pConnectivity = ocSrc->data.SetOfFillAreaSets.connectivity;
    
    for (i = 0 ; i < numFillAreaSets; i++)
    {
	count = pConnectivity->count;
	STORE_CARD16 (count, *ocDest);

	for (j = 0, pList = pConnectivity->lists; j < count; j++, pList++)
	{
	    STORE_CARD16 (pList->count, *ocDest);

	    STORE_LISTOF_CARD16 (pList->count, pList->shorts, *ocDest);
	}
	
	pConnectivity++;
    }

    *ocDest += PAD (cbytes);
}


void _PEXEncodeNURBSurface (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexNURBSurface	*oc;
    pexTrimCurve	*pTCHead;
    PEXTrimCurve	*ptrimCurve;
    PEXListOfTrimCurve	*ptrimLoop;
    int			dataLength;
    int			lenofVertexList;
    int			lenofUKnotList;
    int			lenofVKnotList;
    int			lenofTrimData;
    int			thisLength, i;
    int			count;
    unsigned int	numMPoints, numNPoints; 
    int			rationality, uorder, vorder;
    unsigned long	numTrimLoops;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    numMPoints = ocSrc->data.NURBSurface.col_count;
    numNPoints = ocSrc->data.NURBSurface.row_count;
    rationality = ocSrc->data.NURBSurface.rationality;
    uorder = ocSrc->data.NURBSurface.uorder;
    vorder = ocSrc->data.NURBSurface.vorder;
    numTrimLoops = ocSrc->data.NURBSurface.curve_count;
    
    lenofVertexList = numMPoints * numNPoints *
	((rationality == PEXRational) ?
	LENOF (pexCoord4D) : LENOF (pexCoord3D));
    lenofUKnotList = uorder + numMPoints;
    lenofVKnotList = vorder + numNPoints;
    
    lenofTrimData = numTrimLoops * LENOF (CARD32);
    
    ptrimLoop = ocSrc->data.NURBSurface.trim_curves;
    for (i = 0; i < numTrimLoops; i++, ptrimLoop++)
    {
	ptrimCurve = ptrimLoop->curves;
	count = ptrimLoop->count;
	
	while (count--)
	{
	    lenofTrimData += (LENOF (pexTrimCurve) +
		ptrimCurve->count +
		ptrimCurve->order +     /* knot list */
		ptrimCurve->count *
		(ptrimCurve->rationality == PEXRational ?
		LENOF (pexCoord3D) : LENOF (pexCoord2D)));
	    ptrimCurve++;
	}
    }
    
    dataLength = lenofUKnotList + lenofVKnotList +
	lenofVertexList + lenofTrimData;

    BEGIN_ENCODE_OCHEADER (NURBSurface, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    oc->type = rationality;
    oc->uOrder = uorder;
    oc->vOrder = vorder;
    oc->numUknots = uorder + numMPoints;
    oc->numVknots = vorder + numNPoints;
    oc->mPts = numMPoints;
    oc->nPts = numNPoints;
    oc->numLists = numTrimLoops;
    
    END_ENCODE_OCHEADER (NURBSurface, *ocDest, oc);

    STORE_LISTOF_FLOAT32 (lenofUKnotList, ocSrc->data.NURBSurface.uknots,
	*ocDest, fpConvert, fpFormat);

    STORE_LISTOF_FLOAT32 (lenofVKnotList, ocSrc->data.NURBSurface.vknots,
	*ocDest, fpConvert, fpFormat);

    if (rationality == PEXRational)
    {
	STORE_LISTOF_COORD4D (numMPoints * numNPoints,
            ocSrc->data.NURBSurface.points.point_4d, *ocDest,
	    fpConvert, fpFormat);
    }
    else
    {
	STORE_LISTOF_COORD3D (numMPoints * numNPoints,
	    ocSrc->data.NURBSurface.points.point, *ocDest,
	    fpConvert, fpFormat);
    }
    
    ptrimLoop = ocSrc->data.NURBSurface.trim_curves;
    for (i = 0; i < numTrimLoops; i++, ptrimLoop++)
    {
	count = ptrimLoop->count;
	STORE_CARD32 (count, *ocDest);
	
	ptrimCurve = ptrimLoop->curves;
	
	while (count--)
	{
	    thisLength = ptrimCurve->order + ptrimCurve->count;
	    
	    BEGIN_TRIMCURVE_HEAD (*ocDest, pTCHead);
	    
	    pTCHead->visibility = (pexSwitch) ptrimCurve->visibility;
	    pTCHead->order = (CARD16) ptrimCurve->order;
	    pTCHead->type = (pexCoordType) ptrimCurve->rationality;
	    pTCHead->approxMethod = (INT16) ptrimCurve->approx_method;
	    pTCHead->numKnots = thisLength;
	    pTCHead->numCoord = ptrimCurve->count;

	    if (fpConvert)
	    {
		FP_CONVERT_HTON (ptrimCurve->tolerance,
		    pTCHead->tolerance, fpFormat);
		FP_CONVERT_HTON (ptrimCurve->tmin,
		    pTCHead->tMin, fpFormat);
		FP_CONVERT_HTON (ptrimCurve->tmax,
		    pTCHead->tMax, fpFormat);
	    }
	    else
	    {
		pTCHead->tolerance = (float) ptrimCurve->tolerance;
		pTCHead->tMin = (float) ptrimCurve->tmin;
		pTCHead->tMax = (float) ptrimCurve->tmax;
	    }

	    END_TRIMCURVE_HEAD (*ocDest, pTCHead);
	    *ocDest += SIZEOF (pexTrimCurve);

	    STORE_LISTOF_FLOAT32 (thisLength,
		ptrimCurve->knots.floats, *ocDest, fpConvert, fpFormat);

	    if (ptrimCurve->rationality == PEXRational)
	    {
		STORE_LISTOF_COORD3D (ptrimCurve->count,
		    ptrimCurve->control_points.point, *ocDest,
		    fpConvert, fpFormat);
	    }
	    else
	    {
		STORE_LISTOF_COORD2D (ptrimCurve->count,
		    ptrimCurve->control_points.point_2d, *ocDest,
		    fpConvert, fpFormat);
	    }
	
	    ptrimCurve++;
	}
    }
}


void _PEXEncodeCellArray (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexCellArray	*oc;
    int			dataLength;
    int			count;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    count = ocSrc->data.CellArray.col_count * ocSrc->data.CellArray.row_count;
    dataLength = NUMWORDS (count * SIZEOF (pexTableIndex));
    
    BEGIN_ENCODE_OCHEADER (CellArray, ocSrc->oc_type, dataLength, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.CellArray.point1.x,
	    oc->point1_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.CellArray.point1.y,
	    oc->point1_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.CellArray.point1.z,
	    oc->point1_z, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.CellArray.point2.x,
	    oc->point2_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.CellArray.point2.y,
	    oc->point2_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.CellArray.point2.z,
	    oc->point2_z, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.CellArray.point3.x,
	    oc->point3_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.CellArray.point3.y,
	    oc->point3_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.CellArray.point3.z,
	    oc->point3_z, fpFormat);
    }
    else
    {
	oc->point1_x = ocSrc->data.CellArray.point1.x;
	oc->point1_y = ocSrc->data.CellArray.point1.y;
	oc->point1_z = ocSrc->data.CellArray.point1.z;
	oc->point2_x = ocSrc->data.CellArray.point2.x;
	oc->point2_y = ocSrc->data.CellArray.point2.y;
	oc->point2_z = ocSrc->data.CellArray.point2.z;
	oc->point3_x = ocSrc->data.CellArray.point3.x;
	oc->point3_y = ocSrc->data.CellArray.point3.y;
	oc->point3_z = ocSrc->data.CellArray.point3.z;
    }

    oc->dx = ocSrc->data.CellArray.col_count;
    oc->dy = ocSrc->data.CellArray.row_count;

    END_ENCODE_OCHEADER (CellArray, *ocDest, oc);
    
    STORE_LISTOF_CARD16 (count, ocSrc->data.CellArray.color_indices, *ocDest);

    if (count & 1)
	*ocDest += 2;
}


void _PEXEncodeCellArray2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexCellArray2D	*oc;
    int			dataLength;
    int			count;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    count = ocSrc->data.CellArray2D.col_count *
	ocSrc->data.CellArray2D.row_count;

    dataLength = NUMWORDS (count * SIZEOF (pexTableIndex));
    
    BEGIN_ENCODE_OCHEADER (CellArray2D, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.CellArray2D.point1.x,
	    oc->point1_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.CellArray2D.point1.y,
	    oc->point1_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.CellArray2D.point2.x,
	    oc->point2_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.CellArray2D.point2.y,
	    oc->point2_y, fpFormat);
    }
    else
    {
	oc->point1_x = ocSrc->data.CellArray2D.point1.x;
	oc->point1_y = ocSrc->data.CellArray2D.point1.y;
	oc->point2_x = ocSrc->data.CellArray2D.point2.x;
	oc->point2_y = ocSrc->data.CellArray2D.point2.y;
    }

    oc->dx = ocSrc->data.CellArray2D.col_count;
    oc->dy = ocSrc->data.CellArray2D.row_count;

    END_ENCODE_OCHEADER (CellArray2D, *ocDest, oc);
    
    STORE_LISTOF_CARD16 (count,
	ocSrc->data.CellArray2D.color_indices, *ocDest);

    if (count & 1)
	*ocDest += 2;
}


void _PEXEncodeExtendedCellArray (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexExtendedCellArray	*oc;
    int				count;
    int				dataLength;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    count = ocSrc->data.ExtendedCellArray.col_count *
	ocSrc->data.ExtendedCellArray.row_count;

    dataLength = count *
	GetColorLength (ocSrc->data.ExtendedCellArray.color_type);
    
    BEGIN_ENCODE_OCHEADER (ExtendedCellArray, ocSrc->oc_type,
	dataLength, *ocDest, oc);

    if (fpConvert)
    {
	FP_CONVERT_HTON (ocSrc->data.ExtendedCellArray.point1.x,
	    oc->point1_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.ExtendedCellArray.point1.y,
	    oc->point1_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.ExtendedCellArray.point1.z,
	    oc->point1_z, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.ExtendedCellArray.point2.x,
	    oc->point2_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.ExtendedCellArray.point2.y,
	    oc->point2_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.ExtendedCellArray.point2.z,
	    oc->point2_z, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.ExtendedCellArray.point3.x,
	    oc->point3_x, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.ExtendedCellArray.point3.y,
	    oc->point3_y, fpFormat);
	FP_CONVERT_HTON (ocSrc->data.ExtendedCellArray.point3.z,
	    oc->point3_z, fpFormat);
    }
    else
    {
	oc->point1_x = ocSrc->data.ExtendedCellArray.point1.x;
	oc->point1_y = ocSrc->data.ExtendedCellArray.point1.y;
	oc->point1_z = ocSrc->data.ExtendedCellArray.point1.z;
	oc->point2_x = ocSrc->data.ExtendedCellArray.point2.x;
	oc->point2_y = ocSrc->data.ExtendedCellArray.point2.y;
	oc->point2_z = ocSrc->data.ExtendedCellArray.point2.z;
	oc->point3_x = ocSrc->data.ExtendedCellArray.point3.x;
	oc->point3_y = ocSrc->data.ExtendedCellArray.point3.y;
	oc->point3_z = ocSrc->data.ExtendedCellArray.point3.z;
    }
    
    oc->colorType = ocSrc->data.ExtendedCellArray.color_type;
    oc->dx = ocSrc->data.ExtendedCellArray.col_count;
    oc->dy = ocSrc->data.ExtendedCellArray.row_count;

    END_ENCODE_OCHEADER (ExtendedCellArray, *ocDest, oc);

    STORE_LISTOF_COLOR_VAL (count, ocSrc->data.ExtendedCellArray.color_type,
	ocSrc->data.ExtendedCellArray.colors, *ocDest, fpConvert, fpFormat);
}


void _PEXEncodeGDP (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexGDP	*oc;
    int		dataLength;
    int		fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    dataLength = ocSrc->data.GDP.count * LENOF (pexCoord3D) +
	NUMWORDS (ocSrc->data.GDP.length);
    
    BEGIN_ENCODE_OCHEADER (GDP, ocSrc->oc_type, dataLength, *ocDest, oc);

    oc->gdpId = ocSrc->data.GDP.gdp_id;
    oc->numPoints = ocSrc->data.GDP.count;
    oc->numBytes = ocSrc->data.GDP.length;
    
    END_ENCODE_OCHEADER (GDP, *ocDest, oc);

    STORE_LISTOF_COORD3D (ocSrc->data.GDP.count, ocSrc->data.GDP.points,
	*ocDest, fpConvert, fpFormat);

    memcpy (*ocDest, ocSrc->data.GDP.data, ocSrc->data.GDP.length);
    *ocDest += PADDED_BYTES (ocSrc->data.GDP.length);
}


void _PEXEncodeGDP2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexGDP2D	*oc;
    int		dataLength;
    int		fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    
    dataLength = ocSrc->data.GDP2D.count * LENOF (pexCoord2D) +
	NUMWORDS (ocSrc->data.GDP2D.length);
    
    BEGIN_ENCODE_OCHEADER (GDP2D, ocSrc->oc_type, dataLength, *ocDest, oc);

    oc->gdpId = ocSrc->data.GDP2D.gdp_id;
    oc->numPoints = ocSrc->data.GDP2D.count;
    oc->numBytes = ocSrc->data.GDP2D.length;
    
    END_ENCODE_OCHEADER (GDP2D, *ocDest, oc);

    STORE_LISTOF_COORD2D (ocSrc->data.GDP2D.count, ocSrc->data.GDP2D.points,
	*ocDest, fpConvert, fpFormat);

    memcpy (*ocDest, ocSrc->data.GDP2D.data, ocSrc->data.GDP2D.length);
    *ocDest += PADDED_BYTES (ocSrc->data.GDP2D.length);
}


void _PEXEncodeNoop (fpFormat, ocSrc, ocDest)

int		fpFormat;
PEXOCData	*ocSrc;
char		**ocDest;

{
    pexNoop *oc;

    BEGIN_SIMPLE_ENCODE (Noop, ocSrc->oc_type, *ocDest, oc);
    /* no data */
    END_SIMPLE_ENCODE (Noop, *ocDest, oc);
}
